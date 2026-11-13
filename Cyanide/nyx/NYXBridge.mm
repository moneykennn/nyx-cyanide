#import "NYXBridge.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include "FrameJson.hpp"
#import "CyanideSession.hpp"

namespace {
enum class State { stopped, starting, ready, stopping };
std::atomic<State> state{State::stopped};
std::atomic_bool pending{false};
std::atomic_bool allowReads{false};
std::mutex lock;
std::string ownerKey,frame="{\"status\":\"stopped\",\"lines\":[]}";
std::uint64_t frameStarted=0;
unsigned frameWidth=0,frameHeight=0;
std::unique_ptr<nyx::CyanideSession> session; // worker queue only
dispatch_queue_t queue() {
    static dispatch_queue_t q;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ q=dispatch_queue_create("nyx.capture.serial",DISPATCH_QUEUE_SERIAL); });
    return q;
}
std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool sameOwner(NSString *owner) {
    if (!owner || !owner.UTF8String) return false;
    std::lock_guard<std::mutex> g(lock);
    return ownerKey==owner.UTF8String;
}
void publish(const char* status) {
    std::lock_guard<std::mutex> g(lock);
    frame=std::string("{\"status\":\"")+status+"\",\"lines\":[]}";
    frameStarted=0;frameWidth=0;frameHeight=0;
}
const char* sessionFailure(nyx::SessionStatus s) {
    switch (s) {
        case nyx::SessionStatus::attachFailed:return "attach_failed";
        case nyx::SessionStatus::imageUnavailable:return "image_unavailable";
        case nyx::SessionStatus::unsupportedBuild:return "unsupported_build";
        case nyx::SessionStatus::verifyFailed:return "verify_failed";
        case nyx::SessionStatus::cleanupFailed:return "cleanup_failed";
        default:return "session_error";
    }
}
}

BOOL NYXStart(NSString *owner) {
    if (!owner.length || !owner.UTF8String) return NO;
    State expected=State::stopped;
    if (!state.compare_exchange_strong(expected,State::starting))
        return (expected==State::starting || expected==State::ready) && sameOwner(owner);
    { std::lock_guard<std::mutex> g(lock);ownerKey=owner.UTF8String; }
    publish("starting");
    dispatch_async(queue(), ^{
        @autoreleasepool {
            if (state.load()!=State::starting) return;
            session=std::make_unique<nyx::CyanideSession>();
            const auto result=session->open();
            if (result!=nyx::SessionStatus::ready) {
                const std::string failure=sessionFailure(result);
                session.reset();
                if (state.load()==State::starting) { publish(failure.c_str());state.store(State::stopped); }
                return;
            }
            allowReads.store(true,std::memory_order_release);
            State starting=State::starting;
            if (state.compare_exchange_strong(starting,State::ready)) publish("ready_no_frame");
            else allowReads.store(false,std::memory_order_release);
            // A queued stop owns cleanup if cancellation happened during open.
        }
    });
    return YES;
}

void NYXStop(NSString *owner) {
    if (!sameOwner(owner)) return;
    allowReads.store(false,std::memory_order_release);
    State previous=state.exchange(State::stopping);
    if (previous==State::stopped) { state.store(State::stopped);return; }
    if (previous==State::stopping) return;
    publish("stopping");
    dispatch_async(queue(), ^{
        @autoreleasepool {
            const auto closed=session?session->close():nyx::SessionStatus::stopped;
            session.reset();pending.store(false);
            { std::lock_guard<std::mutex> g(lock);ownerKey.clear(); }
            publish(closed==nyx::SessionStatus::cleanupFailed?"cleanup_failed":"stopped");
            state.store(State::stopped);
        }
    });
}

void NYXStopAll(void) {
    std::string current;
    { std::lock_guard<std::mutex> g(lock);current=ownerKey; }
    if (!current.empty()) NYXStop([NSString stringWithUTF8String:current.c_str()]);
}

NSString *NYXFrame(NSString *owner,unsigned width,unsigned height) {
    if (!sameOwner(owner)) return @"{\"status\":\"not_owner\",\"lines\":[]}";
    if (!width || !height || width>16384 || height>16384)
        return @"{\"status\":\"invalid_viewport\",\"lines\":[]}";
    bool expected=false;
    if (state.load()==State::ready && pending.compare_exchange_strong(expected,true)) {
        dispatch_async(queue(), ^{
            @autoreleasepool {
                if (state.load()==State::ready && session) {
                    const auto started=nowMs();
                    const nyx::Viewport viewport{width,height};
                    const auto snapshot=session->captureSnapshot(viewport,started,&allowReads);
                    auto next=nyx::frameJson(snapshot,viewport,nowMs());
                    if (state.load()==State::ready) {
                        std::lock_guard<std::mutex> g(lock);
                        frame=std::move(next);frameStarted=started;frameWidth=width;frameHeight=height;
                    }
                }
                pending.store(false);
            }
        });
    }
    std::lock_guard<std::mutex> g(lock);
    const auto now=nowMs();
    if (frameStarted && (now<frameStarted || now-frameStarted>250 || width!=frameWidth || height!=frameHeight))
        return @"{\"status\":\"stale\",\"lines\":[]}";
    NSString *result=[NSString stringWithUTF8String:frame.c_str()];
    return result ? result : @"{\"status\":\"invalid\",\"lines\":[]}";
}
