// @param:switch|nyx_remove_probe|Remove test window|false
// Display-only prototype for Cyanide RepoTweaks / QuickLoader.
// No game access, native payload loading, or anti-cheat changes.
(function () {
  "use strict";
  const TAG = 0x4e595850;
  const LIMIT = 128;
  function pointer(value) {
    try { return BigInt(value || 0) !== 0n; } catch (_) { return false; }
  }
  function cls(name) {
    const result = r_class(name);
    if (!pointer(result)) throw new Error("Class unavailable: " + name);
    return result;
  }
  function msg(object, selector, ...args) {
    return r_msg2_main(object, selector, ...args);
  }
  function required(object, description) {
    if (!pointer(object)) throw new Error(description + " unavailable");
    return object;
  }
  const app = required(msg(cls("UIApplication"), "sharedApplication"), "UIApplication");
  const windows = required(msg(app, "windows"), "Window list");
  const count = Number(msg(windows, "count"));
  if (!Number.isInteger(count) || count < 0 || count > LIMIT)
    throw new Error("Unexpected window count; aborting");
  const existing = [];
  let reference = null;
  // Collect first: hiding a window can change UIApplication.windows.
  for (let i = 0; i < count; i++) {
    const window = required(msg(windows, "objectAtIndex:", i), "Window");
    if (Number(msg(window, "tag")) === TAG) existing.push(window);
    else if (!reference && Number(msg(window, "isHidden")) === 0) reference = window;
  }
  for (const window of existing) {
    msg(window, "setHidden:", 1);
    msg(window, "setRootViewController:", "0");
    // Each tagged window owns exactly the allocation retained below.
    msg(window, "release");
  }
  if (globalThis.nyx_remove_probe === true) {
    log("NYX: test windows removed: " + existing.length);
    return;
  }
  required(reference, "Visible host window");
  const frame = required(msg(reference, "valueForKey:", r_nsstr("frame")), "Host frame");
  const scene = msg(reference, "windowScene");
  required(scene, "Host window scene");
  let window = null;
  let controller = null;
  let label = null;
  try {
    window = required(msg(msg(cls("UIWindow"), "alloc"), "initWithWindowScene:", scene), "Test window");
    msg(window, "setTag:", TAG);
    msg(window, "setUserInteractionEnabled:", 0);
    msg(window, "setValue:forKey:", frame, r_nsstr("frame"));
    const level = required(msg(cls("NSNumber"), "numberWithInt:", 2100), "Window level");
    msg(window, "setValue:forKey:", level, r_nsstr("windowLevel"));
    msg(window, "setBackgroundColor:", msg(cls("UIColor"), "clearColor"));
    controller = required(msg(msg(cls("UIViewController"), "alloc"), "init"), "View controller");
    const view = required(msg(controller, "view"), "Root view");
    msg(view, "setBackgroundColor:", msg(cls("UIColor"), "clearColor"));
    msg(view, "setUserInteractionEnabled:", 0);
    label = required(msg(msg(cls("UILabel"), "alloc"), "init"), "Label");
    msg(label, "setText:", r_nsstr("NYX - DISPLAY TEST ONLY"));
    msg(label, "setTextColor:", msg(cls("UIColor"), "whiteColor"));
    msg(label, "setBackgroundColor:", msg(cls("UIColor"), "blackColor"));
    msg(label, "setUserInteractionEnabled:", 0);
    msg(label, "sizeToFit");
    msg(view, "addSubview:", label);
    msg(window, "setRootViewController:", controller);
    // Do not call makeKeyAndVisible: the game must retain keyboard focus.
    msg(window, "setHidden:", 0);
    log("NYX: display test requested. Visibility above games remains unverified.");
  } catch (error) {
    if (pointer(window)) {
      msg(window, "setHidden:", 1);
      msg(window, "setRootViewController:", "0");
      msg(window, "release");
    }
    throw error;
  } finally {
    if (pointer(label)) msg(label, "release");
    if (pointer(controller)) msg(controller, "release");
  }
})();
