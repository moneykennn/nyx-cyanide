const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const code = fs.readFileSync(__dirname + '/../../nyx-repo/esp-experimental.js', 'utf8');
let seq = 0x100000000000001n;
const objects = new Map(), classes = new Map(), events = [], calls = [], timers = [];
function obj(type, state = {}) { const id=String(seq++);objects.set(id,{type,...state});return id; }
const app=obj('app'), scene=obj('scene'), frame=obj('frame'), list=obj('array');
const host=obj('UIWindow',{hidden:0,scene,frame}),windows=[host];
function r_class(name) { if (!classes.has(name)) classes.set(name,obj('class',{name}));return classes.get(name); }
function r_nsstr(value) { return obj('string',{value}); }
function r_sel(value) { return obj('selector',{value}); }
function msg(id,sel,...args) {
  const x=objects.get(id);assert.ok(x,'Unknown remote object '+id);events.push({id,sel,args});
  if (sel==='sharedApplication') return app;
  if (sel==='windows') return list;
  if (sel==='count') return String(windows.length);
  if (sel==='objectAtIndex:') return windows[args[0]];
  if (sel==='tag') return String(x.tag||0);
  if (sel==='isHidden') return String(x.hidden||0);
  if (sel==='windowScene') return x.scene||scene;
  if (sel==='valueForKey:') return frame;
  if (sel==='alloc') return obj(x.name,{hidden:1});
  if (sel==='retain') return id;
  if (sel==='initWithWindowScene:' || sel==='init') return id;
  if (sel==='initWithBase64EncodedString:options:') {
    x.data=Buffer.from(objects.get(args[0]).value,'base64');return id;
  }
  if (sel==='bytes') return obj('bytes',{data:x.data});
  if (sel==='setTag:') {x.tag=args[0];return '0';}
  if (sel==='setHidden:') {
    x.hidden=args[0];if (!x.hidden && !windows.includes(id)) windows.push(id);return '0';
  }
  if (sel==='release') { if (x.type==='UIWindow') { const i=windows.indexOf(id);if(i>=0)windows.splice(i,1); } return '0'; }
  if (sel==='numberWithInt:') return obj('number',{value:args[0]});
  if (sel==='clearColor'||sel==='greenColor') return obj('color');
  if (sel==='CGColor'||sel==='CGPath'||sel==='layer'||sel==='view'||sel==='bezierPath') return obj(sel);
  if (sel==='methodSignatureForSelector:') return obj('signature');
  if (sel==='invocationWithMethodSignature:') return obj('invocation');
  if (sel==='setTarget:') {x.target=args[0];return '0';}
  if (sel==='setSelector:') {x.selector=objects.get(args[0]).value;return '0';}
  if (sel==='setArgument:atIndex:') {x.bytes=objects.get(args[0]).data;return '0';}
  if (sel==='invoke') {
    assert.equal(x.bytes.length,16);calls.push([x.selector,x.bytes.readDoubleLE(0),x.bytes.readDoubleLE(8)]);return '0';
  }
  if (sel.startsWith('set') || sel==='addSublayer:') return '0';
  throw Error('Unexpected selector '+sel);
}
let stops=0;
let frameJson=JSON.stringify({status:'ready',width:667,height:375,lines:[[10,20,30,40]]});
const context={
  r_class,r_nsstr,r_sel,r_msg2_main:msg,
  nyx_start:()=>true,nyx_stop:()=>{stops++},nyx_frame:()=>frameJson,
  log:()=>{},setInterval:fn=>(timers.push(fn),timers.length),clearInterval:()=>{},
  nyx_remove_esp:false,
};
vm.runInNewContext(code,context,{timeout:1000});
assert.equal(windows.length,2);
assert.ok(!events.some(e=>e.sel==='makeKeyAndVisible'));
assert.ok(events.filter(e=>e.sel==='setUserInteractionEnabled:').every(e=>e.args[0]===0));
timers[0]();
assert.deepEqual(calls,[['moveToPoint:',10,20],['addLineToPoint:',30,40]]);
frameJson=JSON.stringify({status:'stale',lines:[]});
timers[0]();
assert.ok(events.some(e=>e.sel==='setPath:' && e.args[0]==='0'));
context.nyx_remove_esp=true;
vm.runInNewContext(code,context,{timeout:1000});
assert.equal(windows.length,1);assert.equal(stops,1);
console.log('PASS: mocked draw commands, stale-frame clearing, focus, touch flags, removal. No iOS runtime tested.');
