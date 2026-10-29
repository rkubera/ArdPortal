// Exercise the real Device format handler while the HTTP operation is pending.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('src/PortalPage.h','utf8');
const strings=JSON.parse(fs.readFileSync('src/languages/en.json','utf8')).strings;
const script=source.slice(source.indexOf('let restartPending=false'),source.indexOf('const deviceNameInput'));
function fixture(confirmed=true){
  const nodes=new Map(),frames=[],requests=[],timers=[],notices=[];
  let focused='';
  const node=id=>{if(!nodes.has(id))nodes.set(id,{hidden:id.endsWith('Overlay'),disabled:false,inert:false,value:0,attrs:{},focus(){focused=id},setAttribute(k,v){this.attrs[k]=v}});return nodes.get(id)};
  const context=vm.createContext({document:{querySelector:node,querySelectorAll:()=>[node('header'),node('main')]},consoleSocket:null,location:{href:''},Date,URLSearchParams,Promise,confirm:()=>confirmed,t:key=>strings[key],showNotice:(...args)=>notices.push(args),requestAnimationFrame:cb=>frames.push(cb),setTimeout:cb=>timers.push(cb),request:(url,options)=>new Promise((resolve,reject)=>requests.push({url,options,resolve,reject,visible:!node('#formatOverlay').hidden}))});
  vm.runInContext(script,context);
  return {node,frames,requests,timers,notices,context,focused:()=>focused};
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
  assert.equal(f.node('#restartOverlay').hidden,true);
  assert.equal(f.timers.length,0); // Formatting has no percentage/countdown timer.
  finishBody('Formatted');await done;
  assert.equal(f.node('#formatOverlay').hidden,true);
  assert.equal(f.node('#restartOverlay').hidden,false);
  assert.equal(vm.runInContext('restartPending',f.context),true);
  for(const bodyFailure of [false,true]){
    const e=fixture(),pending=e.node('#formatStorage').onclick();await paint(e);
    if(bodyFailure)e.requests[0].resolve({text:async()=>{throw Error('read failed')}});
    else e.requests[0].reject(Error('format failed'));
    await pending;
    assert.equal(e.node('#formatOverlay').hidden,true);
    assert.equal(e.node('#restartOverlay').hidden,true);
    assert.equal(e.node('#formatStorage').disabled,false);
    assert.equal(e.node('main').inert,false);
    assert.equal(e.focused(),'#formatStorage');
    assert.equal(e.notices.at(-1)[1],true);
  }
  const cancelled=fixture(false);await cancelled.node('#formatStorage').onclick();
  assert.equal(cancelled.node('#formatOverlay').hidden,true);assert.equal(cancelled.frames.length,0);assert.equal(cancelled.requests.length,0);
  assert.match(source,/animation:formatSweep[^}]*infinite alternate/);
  assert.match(source,/@keyframes formatSweep\{from\{transform:translateX\(0\)\}to\{transform:translateX\(200%\)\}\}/);
  assert.match(source,/class="formatProgress" role="progressbar" data-i18n-label="ui_247"/);
  console.log('PASS: formatting dialog paints before POST, stays indeterminate until completion, blocks duplicates, hands off to restart and recovers from errors');
}
test().catch(error=>{console.error(error);process.exitCode=1});
