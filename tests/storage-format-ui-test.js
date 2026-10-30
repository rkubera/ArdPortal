// Exercise the real Device format handler while the HTTP operation is pending.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('src/PortalPage.h','utf8');
const strings=JSON.parse(fs.readFileSync('src/languages/en.json','utf8')).strings;
const script=source.slice(source.indexOf('let restartPending=false'),source.indexOf('const deviceNameInput'));
function fixture(confirmed=true){
  const nodes=new Map(),frames=[],requests=[],timers=[],intervals=[],probes=[],notices=[];
  let focused='';
  const node=id=>{if(!nodes.has(id))nodes.set(id,{hidden:id.endsWith('Overlay'),disabled:false,inert:false,value:0,attrs:{},focus(){focused=id},setAttribute(k,v){this.attrs[k]=v}});return nodes.get(id)};
  const context=vm.createContext({document:{querySelector:node,querySelectorAll:()=>[node('header'),node('main')]},consoleSocket:null,location:{href:''},Date,URLSearchParams,Promise,confirm:()=>confirmed,t:key=>strings[key],showNotice:(...args)=>notices.push(args),requestAnimationFrame:cb=>frames.push(cb),AbortController,clearTimeout:id=>{timers[id-1].cleared=true},setTimeout:(cb,delay)=>{timers.push({cb,delay});return timers.length},setInterval:(cb,delay)=>{intervals.push({cb,delay});return intervals.length},clearInterval:id=>{intervals[id-1].cleared=true},fetch:(url,options)=>new Promise((resolve,reject)=>{probes.push({url,options,resolve,reject});options.signal.addEventListener('abort',()=>reject(Error('aborted')))}),request:(url,options)=>new Promise((resolve,reject)=>requests.push({url,options,resolve,reject,visible:!node('#formatOverlay').hidden}))});
  vm.runInContext(script,context);
  return {node,frames,requests,timers,intervals,probes,notices,context,focused:()=>focused};
}
const tick=()=>new Promise(setImmediate);
async function paint(f){assert.equal(f.requests.length,0);assert.equal(f.frames.length,1);f.frames.shift()();assert.equal(f.requests.length,0);f.frames.shift()();await tick()}
async function test(){
  const f=fixture(),done=f.node('#formatStorage').onclick();
  assert.equal(f.node('#formatOverlay').hidden,false);
  assert.equal(f.node('main').inert,true);
  assert.equal(f.focused(),'#formatOverlay');
  await f.node('#formatStorage').onclick(); // Repeated input cannot queue another erase.
  await paint(f);
  assert.equal(f.requests.length,1);
  assert.equal(f.requests[0].url,'/api/storage/format');
  assert.equal(f.requests[0].options.body.get('confirm'),'FORMAT');
  assert.equal(f.requests[0].visible,true);
  let finishBody;
  f.requests[0].resolve({text:()=>new Promise(resolve=>finishBody=resolve)});
  await tick();
  assert.equal(f.node('#formatOverlay').hidden,false);
  assert.equal(vm.runInContext('restartPending',f.context),false);
  assert.equal(f.timers.length,0); // Formatting has no percentage/countdown timer.
  finishBody('Formatted');await done;
  assert.equal(f.node('#formatOverlay').hidden,false);
  assert.equal(f.node('#formatTitle').textContent,strings.ui_163);
  assert.equal(f.intervals[0].delay,5000);
  assert.equal(vm.runInContext('restartPending',f.context),true);
  for(const bodyFailure of [false,true]){
    const e=fixture(),pending=e.node('#formatStorage').onclick();await paint(e);
    if(bodyFailure)e.requests[0].resolve({text:async()=>{throw Error('read failed')}});
    else e.requests[0].reject(Error('format failed'));
    await pending;
    assert.equal(e.node('#formatOverlay').hidden,true);
    assert.equal(vm.runInContext('restartPending',e.context),false);
    assert.equal(e.node('#formatStorage').disabled,false);
    assert.equal(e.node('main').inert,false);
    assert.equal(e.focused(),'#formatStorage');
    assert.equal(e.notices.at(-1)[1],true);
  }
  const cancelled=fixture(false);await cancelled.node('#formatStorage').onclick();
  assert.equal(cancelled.node('#formatOverlay').hidden,true);assert.equal(cancelled.frames.length,0);assert.equal(cancelled.requests.length,0);
  assert.match(source,/animation:formatSweep[^}]*infinite alternate/);
  assert.match(source,/@keyframes formatSweep\{from\{transform:translateX\(0\)\}to\{transform:translateX\(200%\)\}\}/);
  assert.match(source,/class="formatProgress" role="progressbar" aria-labelledby="formatTitle"/);
  assert(!source.includes('id="restartOverlay"')&&!source.includes('restartStarted')&&!source.includes('max="60"'));
  assert.equal((source.match(/class="formatProgress"/g)||[]).length,1);
  // Polling waits for boot readiness, ignores the old restarting server and retries errors.
  for(const state of [{configurationReady:false,restarting:false},{configurationReady:true,restarting:true}]){
    const polling=f.intervals[0].cb(),probe=f.probes.at(-1);
    assert.equal(probe.url,'/api/status');assert.equal(probe.options.cache,'no-store');
    probe.resolve({ok:true,json:async()=>state});await polling;
    assert.equal(f.context.location.href,'');assert.equal(f.node('#formatOverlay').hidden,false);
  }
  const offline=f.intervals[0].cb();f.probes.at(-1).reject(Error('offline'));await offline;
  assert.equal(f.context.location.href,'');
  const hung=f.intervals[0].cb();assert.equal(f.timers.at(-1).delay,4000);f.timers.at(-1).cb();await hung;
  assert(f.probes.at(-1).options.signal.aborted);assert.equal(f.context.location.href,'');
  const ready=f.intervals[0].cb();f.probes.at(-1).resolve({ok:true,json:async()=>({configurationReady:true,restarting:false})});await ready;
  assert.equal(f.context.location.href,'/');assert(f.intervals[0].cleared);
  // Explicit restart shares the same dialog and polling without waiting a fixed minute.
  const regular=fixture(),restart=regular.node('#restartDevice').onclick();
  regular.requests[0].resolve({text:async()=> 'Restarting'});await restart;
  assert.equal(regular.node('#formatOverlay').hidden,false);assert.equal(regular.node('main').inert,true);
  assert.equal(regular.intervals[0].delay,5000);
  vm.runInContext('beginRestart()',regular.context);assert.equal(regular.intervals.length,1);
  const reset=fixture(),factory=reset.node('#factoryReset').onclick();
  reset.requests[0].resolve({text:async()=> 'Reset'});await factory;
  assert.equal(reset.node('#formatOverlay').hidden,false);assert.equal(reset.intervals[0].delay,5000);
  console.log('PASS: one shared indeterminate dialog, formatting through reboot, 5s probes, 4s abort, readiness/restarting checks, redirect and error recovery');
}
test().catch(error=>{console.error(error);process.exitCode=1});
