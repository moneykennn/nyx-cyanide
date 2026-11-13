#pragma once

#ifndef __OBJC__
#error "CyanideSession.hpp requires Objective-C++"
#endif
#import <Foundation/Foundation.h>
#import "../TaskRop/RemoteCall.h"
#include "CyanideCaptureAdapter.hpp"
#include <memory>

namespace nyx {
enum class SessionStatus { stopped, ready, attachFailed, imageUnavailable, unsupportedBuild, verifyFailed, cleanupFailed };

// Caller must invoke every method on one background serial queue. Cyanide's
// RemoteCall initialization and teardown may block and must not run on the UI
// thread. The caller must already have started Cyanide's kernel facility.
class CyanideSession final {
    __strong RemoteCallSession* session_ = nil;
    std::unique_ptr<CyanideCaptureAdapter> adapter_;
    SessionStatus status_{SessionStatus::stopped};
public:
    CyanideSession() = default;
    CyanideSession(const CyanideSession&)=delete;
    CyanideSession& operator=(const CyanideSession&)=delete;
    ~CyanideSession() { close(); }

    SessionStatus open() {
        if (session_) return status_;
        RemoteCallSession* candidate = [[RemoteCallSession alloc]
            initWithProcess:@"ShadowTrackerExtra"
            useMigFilterBypass:NO
            firstExceptionTimeoutMS:5000];
        if (!candidate) return status_=SessionStatus::attachFailed;
        session_=candidate;
        // Image index zero is checked against the supplied Mach-O UUID before
        // offsets are used; failure does not activate the reader.
        const std::uint64_t base = [session_
            doRemoteCallStableWithTimeout:5000 functionName:"_dyld_get_image_header"
            x0:0 x1:0 x2:0 x3:0 x4:0 x5:0 x6:0 x7:0];
        if (!readablePointer(base)) return failAndClose(SessionStatus::imageUnavailable);
        adapter_=std::make_unique<CyanideCaptureAdapter>((id<NYXRemoteReadable>)session_,base);
        const auto verified=adapter_->verifyImage();
        if (verified!=CaptureStatus::ready)
            return failAndClose(verified==CaptureStatus::unsupportedBuild
                                ?SessionStatus::unsupportedBuild:SessionStatus::verifyFailed);
        return status_=SessionStatus::ready;
    }

    RawCapture capture(Viewport viewport) const {
        if (status_!=SessionStatus::ready || !adapter_) return {};
        return adapter_->capture(viewport);
    }
    Snapshot captureSnapshot(Viewport viewport,std::uint64_t captureStartedMs,
                             const std::atomic_bool* enabled=nullptr) const {
        if (status_!=SessionStatus::ready || !adapter_) return {};
        return adapter_->captureSnapshot(viewport,captureStartedMs,enabled);
    }
    SessionStatus status() const { return status_; }

    SessionStatus close() {
        adapter_.reset();
        if (!session_) return status_=SessionStatus::stopped;
        const int result=[session_ destroyRemoteCall];
        session_=nil;
        return status_=result==0?SessionStatus::stopped:SessionStatus::cleanupFailed;
    }
private:
    SessionStatus failAndClose(SessionStatus failure) {
        const auto cleanup=close();
        return status_=(cleanup==SessionStatus::cleanupFailed?cleanup:failure);
    }
};
} // namespace nyx
