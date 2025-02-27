// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.media.router.caf;

import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;
import android.support.v7.media.MediaRouter.RouteInfo;

import com.google.android.gms.cast.CastStatusCodes;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.SessionManagerListener;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.DiscoveryCallback;
import org.chromium.chrome.browser.media.router.DiscoveryDelegate;
import org.chromium.chrome.browser.media.router.FlingingController;
import org.chromium.chrome.browser.media.router.MediaRoute;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaRouteProvider;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * A base provider containing common implementation for CAF-based {@link MediaRouteProvider}s.
 */
public abstract class CafBaseMediaRouteProvider
        implements MediaRouteProvider, DiscoveryDelegate, SessionManagerListener<CastSession> {
    private static final String TAG = "CafMR";

    protected static final List<MediaSink> NO_SINKS = Collections.emptyList();
    private final @NonNull MediaRouter mAndroidMediaRouter;
    protected final MediaRouteManager mManager;
    protected final Map<String, DiscoveryCallback> mDiscoveryCallbacks =
            new HashMap<String, DiscoveryCallback>();
    protected final Map<String, MediaRoute> mRoutes = new HashMap<String, MediaRoute>();
    protected Handler mHandler = new Handler();

    private CreateRouteRequestInfo mPendingCreateRouteRequestInfo;

    protected CafBaseMediaRouteProvider(MediaRouter androidMediaRouter, MediaRouteManager manager) {
        mAndroidMediaRouter = androidMediaRouter;
        mManager = manager;
    }

    /**
     * @return A MediaSource object constructed from |sourceId|, or null if the derived class does
     * not support the source.
     */
    @Nullable
    protected abstract MediaSource getSourceFromId(@NonNull String sourceId);

    /**
     * Forward the sinks back to the native counterpart.
     */
    private final void onSinksReceivedInternal(String sourceId, @NonNull List<MediaSink> sinks) {
        Log.d(TAG, "Reporting %d sinks for source: %s", sinks.size(), sourceId);
        mManager.onSinksReceived(sourceId, this, sinks);
    }

    /**
     * {@link DiscoveryDelegate} implementation.
     */
    @Override
    public final void onSinksReceived(String sourceId, @NonNull List<MediaSink> sinks) {
        Log.d(TAG, "Received %d sinks for sourceId: %s", sinks.size(), sourceId);
        mHandler.post(() -> { onSinksReceivedInternal(sourceId, sinks); });
    }

    @Override
    public final boolean supportsSource(String sourceId) {
        return getSourceFromId(sourceId) != null;
    }

    @Override
    public final void startObservingMediaSinks(String sourceId) {
        Log.d(TAG, "startObservingMediaSinks: " + sourceId);

        MediaSource source = getSourceFromId(sourceId);
        if (source == null) {
            // If the source is invalid or not supported by this provider, report no devices
            // available.
            onSinksReceived(sourceId, NO_SINKS);
            return;
        }

        // No-op, if already monitoring the application for this source.
        String applicationId = source.getApplicationId();
        DiscoveryCallback callback = mDiscoveryCallbacks.get(applicationId);
        if (callback != null) {
            callback.addSourceUrn(sourceId);
            return;
        }

        MediaRouteSelector routeSelector = source.buildRouteSelector();
        if (routeSelector == null) {
            // If the application invalid, report no devices available.
            onSinksReceived(sourceId, NO_SINKS);
            return;
        }

        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        for (RouteInfo route : mAndroidMediaRouter.getRoutes()) {
            if (route.matchesSelector(routeSelector)) {
                knownSinks.add(MediaSink.fromRoute(route));
            }
        }

        callback = new DiscoveryCallback(sourceId, knownSinks, this, routeSelector);
        mAndroidMediaRouter.addCallback(
                routeSelector, callback, MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY);
        mDiscoveryCallbacks.put(applicationId, callback);
    }

    @Override
    public final void stopObservingMediaSinks(String sourceId) {
        Log.d(TAG, "startObservingMediaSinks: " + sourceId);

        MediaSource source = getSourceFromId(sourceId);
        if (source == null) return;

        // No-op, if already monitoring the application for this source.
        String applicationId = source.getApplicationId();
        DiscoveryCallback callback = mDiscoveryCallbacks.get(applicationId);
        if (callback == null) return;

        callback.removeSourceUrn(sourceId);

        if (callback.isEmpty()) {
            mAndroidMediaRouter.removeCallback(callback);
            mDiscoveryCallbacks.remove(applicationId);
        }
    }

    @Override
    public final void createRoute(String sourceId, String sinkId, String presentationId,
            String origin, int tabId, boolean isIncognito, int nativeRequestId) {
        Log.d(TAG, "createRoute");
        CastUtils.getCastContext().getSessionManager().addSessionManagerListener(
                this, CastSession.class);
        if (mPendingCreateRouteRequestInfo != null) {
            // TODO(zqzhang): do something.
        }

        MediaSink sink = MediaSink.fromSinkId(sinkId, mAndroidMediaRouter);
        if (sink == null) {
            mManager.onRouteRequestError("No sink", nativeRequestId);
            return;
        }

        MediaSource source = getSourceFromId(sourceId);
        if (source == null) {
            mManager.onRouteRequestError("Unsupported source URL", nativeRequestId);
            return;
        }

        MediaRouter.RouteInfo targetRouteInfo = null;
        for (MediaRouter.RouteInfo routeInfo : getAndroidMediaRouter().getRoutes()) {
            if (routeInfo.getId().equals(sink.getId())) {
                targetRouteInfo = routeInfo;
                break;
            }
        }
        if (targetRouteInfo == null) {
            mManager.onRouteRequestError("The sink does not exist", nativeRequestId);
        }

        mPendingCreateRouteRequestInfo = new CreateRouteRequestInfo(source, sink, presentationId,
                origin, tabId, isIncognito, nativeRequestId, targetRouteInfo);

        sessionController().requestSessionLaunch();
    }

    @Override
    public void closeRoute(String routeId) {
        MediaRoute route = mRoutes.get(routeId);
        if (route == null) return;

        if (!hasSession()) {
            removeRoute(routeId, /* error= */ null);
            return;
        }

        sessionController().endSession();
    }

    @Override
    public void detachRoute(String routeId) {
        removeRoute(routeId, /* error= */ null);
    }

    ///////////////////////////////////////////////////////
    // SessionManagerListener implementation begin
    ///////////////////////////////////////////////////////

    @Override
    public final void onSessionStarting(CastSession session) {
        // The session is not connected yet at this point so this is no-op.
    }

    @Override
    public void onSessionStartFailed(CastSession session, int error) {
        removeAllRoutesWithError("Launch error");
        mPendingCreateRouteRequestInfo = null;
    }

    @Override
    public void onSessionStarted(CastSession session, String sessionId) {
        Log.d(TAG, "onSessionStarted");
        sessionController().attachToCastSession(session);
        sessionController().onSessionStarted();

        MediaSink sink = mPendingCreateRouteRequestInfo.sink;
        MediaSource source = mPendingCreateRouteRequestInfo.source;
        MediaRoute route = new MediaRoute(
                sink.getId(), source.getSourceId(), mPendingCreateRouteRequestInfo.presentationId);
        addRoute(route, mPendingCreateRouteRequestInfo.origin, mPendingCreateRouteRequestInfo.tabId,
                mPendingCreateRouteRequestInfo.nativeRequestId, /* wasLaunched= */ true);

        mPendingCreateRouteRequestInfo = null;
    }

    @Override
    public final void onSessionResumed(CastSession session, boolean wasSuspended) {
        sessionController().attachToCastSession(session);
    }

    @Override
    public final void onSessionResuming(CastSession session, String sessionId) {}

    @Override
    public final void onSessionResumeFailed(CastSession session, int error) {}

    @Override
    public final void onSessionEnding(CastSession session) {
        handleSessionEnd(/* error= */ null);
    }

    @Override
    public final void onSessionEnded(CastSession session, int error) {
        handleSessionEnd((error == CastStatusCodes.SUCCESS)
                        ? null
                        : ("Session ended with error code " + error));
    }

    @Override
    public final void onSessionSuspended(CastSession session, int reason) {
        sessionController().detachFromCastSession();
    }

    ///////////////////////////////////////////////////////
    // SessionManagerListener implementation end
    ///////////////////////////////////////////////////////

    private void handleSessionEnd(String error) {
        if (mPendingCreateRouteRequestInfo != null) {
            // The Cast SDK notifies about session ending when a route is unselected, even when
            // there's no current session. Because CastSessionController unselects the route to set
            // the receiver app ID, this needs to be guarded by a pending request null check to make
            // sure the listener is not unregistered during a session relaunch.
            return;
        }
        sessionController().onSessionEnded();
        sessionController().detachFromCastSession();
        getAndroidMediaRouter().selectRoute(getAndroidMediaRouter().getDefaultRoute());
        removeAllRoutesWithError(error);
        CastUtils.getCastContext().getSessionManager().removeSessionManagerListener(
                this, CastSession.class);
    }

    public @NonNull MediaRouter getAndroidMediaRouter() {
        return mAndroidMediaRouter;
    }

    protected boolean hasSession() {
        return sessionController().isConnected();
    }

    abstract public BaseSessionController sessionController();

    public CafMessageHandler getMessageHandler() {
        return null;
    }

    /** Adds a route for bookkeeping. */
    protected void addRoute(
            MediaRoute route, String origin, int tabId, int nativeRequestId, boolean wasLaunched) {
        mRoutes.put(route.id, route);
        mManager.onRouteCreated(route.id, route.sinkId,
                sessionController().getRouteCreationInfo().nativeRequestId, this, wasLaunched);
    }

    /** Removes a route for bookkeeping. A null {@code error} indicates no error. */
    protected void removeRoute(String routeId, @Nullable String error) {
        mRoutes.remove(routeId);
        if (error == null) {
            mManager.onRouteClosed(routeId);
        } else {
            mManager.onRouteClosedWithError(routeId, error);
        }
    }

    /** Removes all routes for bookkeeping. A null {@code error} indicates no error. */
    protected void removeAllRoutesWithError(@Nullable String error) {
        Set<String> routeIds = new HashSet<>(mRoutes.keySet());
        for (String routeId : routeIds) {
            removeRoute(routeId, error);
        }
    }

    @Override
    @Nullable
    public FlingingController getFlingingController(String routeId) {
        return null;
    }

    public CreateRouteRequestInfo getPendingCreateRouteRequestInfo() {
        return mPendingCreateRouteRequestInfo;
    }
}
