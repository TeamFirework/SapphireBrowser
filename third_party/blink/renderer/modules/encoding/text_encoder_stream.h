// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ENCODING_TEXT_ENCODER_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ENCODING_TEXT_ENCODER_STREAM_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/transform_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ScriptState;
class Visitor;

// Implements the TextDecoderStream interface as specified at
// https://encoding.spec.whatwg.org/#interface-textencoderstream.
// Converts a stream of text data in the form of string chunks to a stream of
// binary data in the form of UInt8Array chunks. After construction
// functionality is delegated to the owned TransformStream.
class TextEncoderStream final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TextEncoderStream* Create(ScriptState*, ExceptionState&);
  ~TextEncoderStream() override;

  // From text_encoder_stream.idl
  String encoding() const;
  ScriptValue readable(ScriptState*, ExceptionState&) const;
  ScriptValue writable(ScriptState*, ExceptionState&) const;

  void Trace(Visitor* visitor) override;

 private:
  class Transformer;
  static void Noop(ScriptValue);

  TextEncoderStream(ScriptState*, ExceptionState&);

  // We need to keep the wrapper alive in order to make
  // |transform_| alive. We can create a wrapper in the constructor, but there
  // is a chance that GC happens after construction happens before the wrapper
  // is connected to the value returned to the user in the JS world. This
  // function posts a task with a ScriptPromise containing the wrapper to avoid
  // that.
  // TODO(ricea): Remove this once the unified GC is available.
  void RetainWrapperUntilV8WrapperGetReturnedToV8(ScriptState*);

  const TraceWrapperMember<TransformStream> transform_;

  DISALLOW_COPY_AND_ASSIGN(TextEncoderStream);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ENCODING_TEXT_ENCODER_STREAM_H_
