#pragma once

#ifndef __OBJC__
#error "CyanideCaptureAdapter.hpp requires Objective-C++"
#endif
#import <Foundation/Foundation.h>
#include "PlayerReader.hpp"
#include <atomic>

// Matches the public selector on Cyanide v1.17.0 RemoteCallSession.
// An existing, caller-owned session is required. This adapter does not create
// an exploit, select a process, or expose any method to RepoTweaks JavaScript.
@protocol NYXRemoteReadable <NSObject>
- (BOOL)remoteRead:(uint64_t)source to:(void *)destination size:(uint64_t)size;
@end

namespace nyx {
class CyanideCaptureAdapter final {
    __strong id<NYXRemoteReadable> session_;
    std::uint64_t imageBase_;
    mutable PlayerReaderCache cache_;
    struct Reader {
        __unsafe_unretained id<NYXRemoteReadable> session;
        const std::atomic_bool* enabled{};
        bool read(std::uint64_t address,void* buffer,std::size_t size) const {
            if (!session || !buffer || !size || size>1024*1024
                || (enabled && !enabled->load(std::memory_order_acquire))) return false;
            if (address>std::numeric_limits<std::uint64_t>::max()-size) return false;
            return [session remoteRead:address to:buffer size:static_cast<uint64_t>(size)] == YES;
        }
    };
public:
    CyanideCaptureAdapter(id<NYXRemoteReadable> session,std::uint64_t imageBase)
        : session_(session),imageBase_(imageBase) {}
    CyanideCaptureAdapter(const CyanideCaptureAdapter&)=delete;
    CyanideCaptureAdapter& operator=(const CyanideCaptureAdapter&)=delete;
    CaptureStatus verifyImage() const {
        if (!session_) return CaptureStatus::unavailable;
        Reader reader{session_};
        return nyx::verifyBuild(reader,imageBase_);
    }
    RawCapture capture(Viewport viewport) const {
        if (!session_) return {};
        Reader reader{session_};
        return nyx::capture(reader,imageBase_,viewport);
    }
    Snapshot captureSnapshot(Viewport viewport,std::uint64_t captureStartedMs,
                             const std::atomic_bool* enabled=nullptr) const {
        if (!session_) return {};
        Reader reader{session_,enabled};
        const auto raw=nyx::capture(reader,imageBase_,viewport);
        return buildPlayerSnapshot(reader,imageBase_,raw,viewport,captureStartedMs,&cache_);
    }
};
} // namespace nyx
