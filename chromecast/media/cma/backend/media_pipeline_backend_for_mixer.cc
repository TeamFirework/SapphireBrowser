// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"

#include <time.h>
#include <limits>

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"
#include "chromecast/media/cma/backend/av_sync.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"

#if defined(OS_LINUX)
#include "chromecast/media/cma/backend/audio_buildflags.h"
#endif  // defined(OS_LINUX)

#if defined(OS_FUCHSIA)
#include <zircon/syscalls.h>
#endif  // defined(OS_FUCHSIA)

namespace {
int64_t kSyncedPlaybackStartDelayUs = 0;
}  // namespace

namespace chromecast {
namespace media {

MediaPipelineBackendForMixer::MediaPipelineBackendForMixer(
    const MediaPipelineDeviceParams& params)
    : state_(kStateUninitialized), params_(params) {}

MediaPipelineBackendForMixer::~MediaPipelineBackendForMixer() {}

MediaPipelineBackendForMixer::AudioDecoder*
MediaPipelineBackendForMixer::CreateAudioDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    return nullptr;
  audio_decoder_ = std::make_unique<AudioDecoderForMixer>(this);
  if (video_decoder_ && !av_sync_ && !IsIgnorePtsMode()) {
    av_sync_ = AvSync::Create(GetTaskRunner(), this);
  }
  return audio_decoder_.get();
}

MediaPipelineBackendForMixer::VideoDecoder*
MediaPipelineBackendForMixer::CreateVideoDecoder() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (video_decoder_)
    return nullptr;
  video_decoder_ = VideoDecoderForMixer::Create(params_);
  video_decoder_->SetObserver(this);
  DCHECK(video_decoder_.get());
  if (audio_decoder_ && !av_sync_ && !IsIgnorePtsMode()) {
    av_sync_ = AvSync::Create(GetTaskRunner(), this);
  }
  return video_decoder_.get();
}

bool MediaPipelineBackendForMixer::Initialize() {
  DCHECK_EQ(kStateUninitialized, state_);
  if (audio_decoder_)
    audio_decoder_->Initialize();
  if (video_decoder_ && !video_decoder_->Initialize()) {
    return false;
  }
  state_ = kStateInitialized;
  return true;
}

bool MediaPipelineBackendForMixer::Start(int64_t start_pts) {
  DCHECK_EQ(kStateInitialized, state_);

  video_ready_to_play_ = !video_decoder_;
  audio_ready_to_play_ = !audio_decoder_;

  start_playback_timestamp_us_ = INT64_MIN;
  start_playback_pts_us_ = start_pts;

  int64_t effective_start_pts =
      (IsIgnorePtsMode() ? INT64_MIN : start_playback_pts_us_);
  bool start_playback_asap = !av_sync_;
  if (audio_decoder_ &&
      !audio_decoder_->Start(effective_start_pts, start_playback_asap)) {
    return false;
  }

  if (video_decoder_ && !video_decoder_->Start(start_playback_pts_us_, true))
    return false;

  state_ = kStatePlaying;
  playback_started_ = !av_sync_;
  starting_playback_rate_ = 1.0;

  return true;
}

void MediaPipelineBackendForMixer::Stop() {
  DCHECK(state_ == kStatePlaying || state_ == kStatePaused)
      << "Invalid state " << state_;
  if (audio_decoder_)
    audio_decoder_->Stop();
  if (video_decoder_)
    video_decoder_->Stop();
  if (av_sync_) {
    av_sync_->NotifyStop();
  }

  state_ = kStateInitialized;
}

bool MediaPipelineBackendForMixer::Pause() {
  DCHECK_EQ(kStatePlaying, state_);
  if (!playback_started_) {
    state_ = kStatePaused;
    LOG(INFO) << "Pause received while playback has not started yet.";
    return true;
  }

  if (audio_decoder_ && !audio_decoder_->Pause()) {
    return false;
  }
  if (video_decoder_ && !video_decoder_->Pause()) {
    return false;
  }
  if (av_sync_) {
    av_sync_->NotifyPause();
  }

  state_ = kStatePaused;
  return true;
}

bool MediaPipelineBackendForMixer::Resume() {
  DCHECK_EQ(kStatePaused, state_);

  if (!playback_started_) {
    LOG(INFO) << "Resume received while playback has not started yet.";
    state_ = kStatePlaying;
    TryStartPlayback();
    return true;
  }

  if (av_sync_) {
    av_sync_->NotifyResume();
  }
  if (audio_decoder_ && !audio_decoder_->Resume())
    return false;
  if (video_decoder_ && !video_decoder_->Resume())
    return false;

  state_ = kStatePlaying;
  return true;
}

bool MediaPipelineBackendForMixer::SetPlaybackRate(float rate) {
  if (!playback_started_) {
    LOG(INFO) << "Got playback rate change before playback has started.";

    // Some vendor VideoDecoderForMixer implementations may not properly handle
    // the rate change before playback has started. It needs to be moved to
    // after we start playback
    starting_playback_rate_ = rate;
  } else {
    LOG(INFO) << __func__ << " rate=" << rate;

    // If av_sync_ is available, only change the audio rate of playback. This
    // will notify us of when the audio playback rate change goes into effect,
    // and then we'll change the video rate of playback.
    if (av_sync_) {
      DCHECK(audio_decoder_);
      audio_decoder_->SetPlaybackRate(rate);
      return true;
    }

    if (audio_decoder_) {
      rate = audio_decoder_->SetPlaybackRate(rate);
    }
    if (video_decoder_ && !video_decoder_->SetPlaybackRate(rate))
      return false;
  }

  return true;
}

int64_t MediaPipelineBackendForMixer::GetCurrentPts() {
  int64_t timestamp = 0;
  int64_t pts = 0;
  if (video_decoder_ && video_decoder_->GetCurrentPts(&timestamp, &pts))
    return pts;
  if (audio_decoder_)
    return audio_decoder_->GetCurrentPts();
  return std::numeric_limits<int64_t>::min();
}

bool MediaPipelineBackendForMixer::Primary() const {
  return (params_.audio_type !=
          MediaPipelineDeviceParams::kAudioStreamSoundEffects);
}

std::string MediaPipelineBackendForMixer::DeviceId() const {
  return params_.device_id;
}

AudioContentType MediaPipelineBackendForMixer::ContentType() const {
  return params_.content_type;
}

AudioChannel MediaPipelineBackendForMixer::AudioChannel() const {
  return params_.audio_channel;
}

const scoped_refptr<base::SingleThreadTaskRunner>&
MediaPipelineBackendForMixer::GetTaskRunner() const {
  return static_cast<TaskRunnerImpl*>(params_.task_runner)->runner();
}

#if defined(OS_LINUX)
int64_t MediaPipelineBackendForMixer::MonotonicClockNow() const {
  timespec now = {0, 0};
#if BUILDFLAG(MEDIA_CLOCK_MONOTONIC_RAW)
  clock_gettime(CLOCK_MONOTONIC_RAW, &now);
#else
  clock_gettime(CLOCK_MONOTONIC, &now);
#endif // MEDIA_CLOCK_MONOTONIC_RAW
  return static_cast<int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}
#elif defined(OS_FUCHSIA)
int64_t MediaPipelineBackendForMixer::MonotonicClockNow() const {
  return zx_clock_get(ZX_CLOCK_MONOTONIC) / 1000;
}
#endif

bool MediaPipelineBackendForMixer::IsIgnorePtsMode() const {
  return params_.sync_type ==
             MediaPipelineDeviceParams::MediaSyncType::kModeIgnorePts ||
         params_.sync_type ==
             MediaPipelineDeviceParams::MediaSyncType::kModeIgnorePtsAndVSync;
}

void MediaPipelineBackendForMixer::VideoReadyToPlay() {
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaPipelineBackendForMixer::OnVideoReadyToPlay,
                     base::Unretained(this)));
}

void MediaPipelineBackendForMixer::OnVideoReadyToPlay() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!video_ready_to_play_);
  DCHECK(video_decoder_);

  LOG(INFO) << "Video ready to play";
  video_ready_to_play_ = true;

  if (av_sync_) {
    TryStartPlayback();
  } else if (!IsIgnorePtsMode()) {
    start_playback_timestamp_us_ = MonotonicClockNow();
    LOG(INFO) << "Starting playback at=" << start_playback_timestamp_us_;

    video_decoder_->SetPts(start_playback_timestamp_us_,
                           start_playback_pts_us_);
  }
}

void MediaPipelineBackendForMixer::OnAudioReadyForPlayback() {
  DCHECK(!audio_ready_to_play_);

  LOG(INFO) << "Audio ready to play";
  audio_ready_to_play_ = true;

  if (av_sync_) {
    TryStartPlayback();
  }
}

void MediaPipelineBackendForMixer::TryStartPlayback() {
  DCHECK(av_sync_);
  DCHECK(!IsIgnorePtsMode());
  DCHECK(video_decoder_);
  DCHECK(audio_decoder_);
  DCHECK(!playback_started_);

  if (!audio_ready_to_play_ || !video_ready_to_play_ ||
      state_ != kStatePlaying) {
    return;
  }

  start_playback_timestamp_us_ =
      MonotonicClockNow() + kSyncedPlaybackStartDelayUs;
  LOG(INFO) << "Starting playback at=" << start_playback_timestamp_us_;

  video_decoder_->SetPts(start_playback_timestamp_us_, start_playback_pts_us_);
  audio_decoder_->StartPlaybackAt(start_playback_timestamp_us_);
  av_sync_->NotifyStart(start_playback_timestamp_us_, start_playback_pts_us_);

  playback_started_ = true;
  if (starting_playback_rate_ != 1.0) {
    SetPlaybackRate(starting_playback_rate_);
  }
}

void MediaPipelineBackendForMixer::NewAudioPlaybackRateInEffect(float rate) {
  if (av_sync_) {
    av_sync_->NotifyPlaybackRateChange(rate);
  }
}

}  // namespace media
}  // namespace chromecast
