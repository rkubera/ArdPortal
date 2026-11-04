// Exercise the shipped password input and save completion behavior.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../src/PortalPage.h'),'utf8');
const start=source.indexOf("    const node=dynamicElement(field.type==='select'");
const end=source.indexOf('binding.node=node;',start)+'binding.node=node;'.length;
assert(start>=0&&end>start);
function input(type){
 const node={value:'',setCustomValidity(v){this.validity=v},reportValidity(){return !this.validity}};
 const binding={},sent=[],timers=[];
 vm.runInNewContext(source.slice(start,end),{node,field:{id:'token',type,maxLength:1024},binding,label:{},page:{id:'secrets'},
  dynamicElement:()=>node,TextEncoder,queueDynamic:(b,p,c,v)=>sent.push(v),clearTimeout(){},setTimeout:f=>timers.push(f)});
 return {node,binding,sent,timers};
}
const p=input('password');
assert.equal(p.node.type,'password');assert.equal(p.node.autocomplete,'new-password');assert.equal(p.node.maxLength,1024);
p.node.value='private password';p.node.oninput();assert(p.binding.dirty);assert.equal(p.timers.length,0);assert.equal(p.sent.length,0);
p.node.onchange();assert.deepEqual(p.sent,['private password']);
p.binding.write('stored value');assert.equal(p.node.value,'');
p.node.onchange();assert.equal(p.sent.length,1);assert.equal(p.binding.dirty,false);
const edit=input('edit');assert.equal(edit.node.type,'text');edit.node.value='label';edit.node.oninput();assert.equal(edit.timers.length,1);
const flushStart=source.indexOf('async function flushDynamic()'),flushEnd=source.indexOf("let appError=",flushStart);
async function saved(typed){
 const binding={field:{id:'token',type:'password'},version:1,dirty:true,node:{value:typed},read(){return this.node.value},write(v){this.node.value=v}};
 const job={key:'secrets/token/value',binding,page:{id:'secrets'},control:undefined,value:'first secret',version:1};
 const ctx=vm.createContext({dynamicQueue:new Map([[job.key,job]]),dynamicSending:false,request:async()=>({text:async()=>''}),refreshAppFields:async()=>{},setTimeout(){},showNotice(){}});
 await vm.runInContext(source.slice(flushStart,flushEnd)+'\nflushDynamic()',ctx);
 return binding;
}
module.exports=(async()=>{
 const settled=await saved('first secret');assert.equal(settled.node.value,'');assert.equal(settled.dirty,false);
 const newer=await saved('second secret');assert.equal(newer.node.value,'second secret');assert.equal(newer.dirty,true);
 console.log('PASS: masked password input, blur-only save, empty preservation and newer edits survive older save completion');
})();
module.exports.catch(error=>{console.error(error);process.exitCode=1});
