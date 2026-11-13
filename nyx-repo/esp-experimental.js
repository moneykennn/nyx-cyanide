// @param:switch|nyx_remove_esp|Remove Nyx and stop reading|false
// Experimental Cyanide-Nyx fork only. Not compatible with stock Cyanide.
// No anti-ban guarantee. Device performance and bone offsets are unverified.
(function () {
  "use strict";
  const tag=0x4e595851, width=667, height=375, lineLimit=96;
  function valid(p) { try { return BigInt(p||0)!==0n; } catch (_) { return false; } }
  function requirePtr(p,name) { if (!valid(p)) throw Error(name+" unavailable");return p; }
  function msg(o,s,...a) { return r_msg2_main(o,s,...a); }
  function klass(n) { return requirePtr(r_class(n),n); }
  function ns(s) { return requirePtr(r_nsstr(s),"NSString"); }
  function key(o,s,k) { const v=ns(k);try { return msg(o,s,v); } finally { msg(v,"release"); } }
  function setKey(o,v,k) { const keyName=ns(k);try { return msg(o,"setValue:forKey:",v,keyName); } finally { msg(keyName,"release"); } }
  function base64(bytes) {
    const chars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let out="";
    for (let i=0;i<bytes.length;i+=3) {
      const value=(bytes[i]<<16)|((bytes[i+1]||0)<<8)|(bytes[i+2]||0);
      out+=chars[(value>>>18)&63]+chars[(value>>>12)&63]+
           (i+1<bytes.length?chars[(value>>>6)&63]:"=")+(i+2<bytes.length?chars[value&63]:"=");
    }
    return out;
  }
  // NSInvocation transports CGPoint's two float64 fields using remote NSData.
  // r_msg2_main itself only transports integer/pointer arguments.
  function invokePoint(inv,x,y) {
    const bytes=new Uint8Array(16),view=new DataView(bytes.buffer);
    view.setFloat64(0,x,true);view.setFloat64(8,y,true);
    const text=ns(base64(bytes));let data="0";
    try {
      data=requirePtr(msg(msg(klass("NSData"),"alloc"),"initWithBase64EncodedString:options:",text,0),"point data");
      const ptr=requirePtr(msg(data,"bytes"),"point bytes");
      msg(inv,"setArgument:atIndex:",ptr,2);msg(inv,"invoke");
    } finally { if (valid(data)) msg(data,"release");msg(text,"release"); }
  }
  function invocation(target,selector) {
    const sel=requirePtr(r_sel(selector),"selector");
    const signature=requirePtr(msg(target,"methodSignatureForSelector:",sel),"method signature");
    const inv=requirePtr(msg(klass("NSInvocation"),"invocationWithMethodSignature:",signature),"invocation");
    msg(inv,"setTarget:",target);msg(inv,"setSelector:",sel);return inv;
  }
  const app=requirePtr(msg(klass("UIApplication"),"sharedApplication"),"application");
  const windows=requirePtr(msg(app,"windows"),"windows");
  const count=Number(msg(windows,"count"));
  if (!Number.isInteger(count)||count<0||count>128) throw Error("Invalid window count");
  let host=null;const old=[];
  for (let i=0;i<count;++i) {
    const w=requirePtr(msg(windows,"objectAtIndex:",i),"window");
    if (Number(msg(w,"tag"))===tag) old.push(w);
    else if (!host && Number(msg(w,"isHidden"))===0) host=w;
  }
  for (const w of old) { msg(w,"setHidden:",1);msg(w,"setRootViewController:","0");msg(w,"release"); }
  if (globalThis.nyx_remove_esp===true) { nyx_stop();log("Nyx stopped; window removed");return; }
  requirePtr(host,"visible window");
  const frame=requirePtr(key(host,"valueForKey:","frame"),"frame");
  const scene=requirePtr(msg(host,"windowScene"),"scene");
  let window="0",controller="0",shape="0";
  try {
    window=requirePtr(msg(msg(klass("UIWindow"),"alloc"),"initWithWindowScene:",scene),"window");
    msg(window,"setTag:",tag);msg(window,"setUserInteractionEnabled:",0);
    setKey(window,frame,"frame");
    setKey(window,msg(klass("NSNumber"),"numberWithInt:",2100),"windowLevel");
    msg(window,"setBackgroundColor:",msg(klass("UIColor"),"clearColor"));
    controller=requirePtr(msg(msg(klass("UIViewController"),"alloc"),"init"),"controller");
    const view=requirePtr(msg(controller,"view"),"view");
    msg(view,"setBackgroundColor:",msg(klass("UIColor"),"clearColor"));
    msg(view,"setUserInteractionEnabled:",0);
    shape=requirePtr(msg(klass("CAShapeLayer"),"layer"),"shape");
    setKey(shape,requirePtr(key(window,"valueForKey:","bounds"),"window bounds"),"frame");
    const green=msg(klass("UIColor"),"greenColor");
    msg(shape,"setStrokeColor:",requirePtr(msg(green,"CGColor"),"stroke color"));
    msg(shape,"setFillColor:","0");
    setKey(shape,msg(klass("NSNumber"),"numberWithInt:",1),"lineWidth");
    msg(requirePtr(msg(view,"layer"),"layer"),"addSublayer:",shape);
    msg(window,"setRootViewController:",controller);
    msg(window,"setHidden:",0); // Do not steal key-window focus.
    msg(controller,"release");controller="0";
  } catch (e) {
    if (valid(controller)) msg(controller,"release");
    if (valid(window)) {msg(window,"setHidden:",1);msg(window,"setRootViewController:","0");msg(window,"release");}
    nyx_stop();throw e;
  }
  let attempts=0,started=false,lastStatus="";
  const timer=setInterval(function () {
    try {
      if (!started) {
        if (!nyx_start()) { if (++attempts>=20) {clearInterval(timer);log("Nyx: start unavailable");} return; }
        started=true;
      }
      const frame=JSON.parse(nyx_frame(width,height));
      if (frame.status!==lastStatus) {lastStatus=frame.status;log("Nyx: "+lastStatus);}
      if (frame.status!=="ready" || frame.width!==width || frame.height!==height ||
          !Array.isArray(frame.lines) || frame.lines.length>lineLimit) {
        msg(shape,"setPath:","0");return;
      }
      const path=requirePtr(msg(klass("UIBezierPath"),"bezierPath"),"path");
      msg(path,"retain");let move="0",line="0";
      try {
        move=invocation(path,"moveToPoint:");msg(move,"retain");
        line=invocation(path,"addLineToPoint:");msg(line,"retain");
        for (const edge of frame.lines) {
          if (!Array.isArray(edge)||edge.length!==4 || !edge.every(Number.isFinite)) continue;
          invokePoint(move,edge[0],edge[1]);invokePoint(line,edge[2],edge[3]);
        }
        msg(shape,"setPath:",msg(path,"CGPath"));
      } finally {
        if (valid(move)) msg(move,"release");if (valid(line)) msg(line,"release");msg(path,"release");
      }
    } catch (e) {
      clearInterval(timer);nyx_stop();msg(shape,"setPath:","0");log("Nyx error: "+String(e));
    }
  },100);
})();
