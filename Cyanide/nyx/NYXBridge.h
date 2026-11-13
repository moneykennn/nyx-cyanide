#pragma once
#import <Foundation/Foundation.h>

// Experimental native bridge. Calls only schedule work; none blocks the JS/UI
// thread. The owner is the RepoTweaks storage key, not an arbitrary process.
#ifdef __cplusplus
extern "C" {
#endif
BOOL NYXStart(NSString *owner);
void NYXStop(NSString *owner);
void NYXStopAll(void);
NSString *NYXFrame(NSString *owner, unsigned width, unsigned height);
#ifdef __cplusplus
}
#endif
