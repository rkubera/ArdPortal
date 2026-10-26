// Exercise the actual browser refresh scheduler with deferred HTTP reads.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('src/PortalPage.h','utf8');
const start=source.indexOf('function refreshAppFields('),end=source.indexOf('\n/**',start);
let resolveRead,calls=0,fail=false;
const ctx=vm.createContext({restartPending:false,dynamicBindings:[{}],dynamicDefinitions:[],activeDynamicId:'a',appRead:null,appReadAgain:false,appPendingRevision:null,appSnapshotPage:null,appSnapshotRevision:null,
 readAppFields(){++calls;const page=ctx.activeDynamicId;return new Promise((resolve,reject)=>{resolveRead=(revision)=>{if(fail){fail=false;reject(Error('offline'));return}ctx.appSnapshotPage=page;ctx.appSnapshotRevision=revision;resolve()}})}});
vm.runInContext(source.slice(start,end),ctx);
const settle=async()=>{for(let i=0;i<8;i++)await Promise.resolve()};
(async()=>{
 let first=ctx.refreshAppFields();ctx.refreshAppFields();ctx.refreshAppFields(1);assert.equal(calls,1);
 resolveRead(1);await first;await settle();assert.equal(calls,1);
 await ctx.refreshAppFields(1);assert.equal(calls,1);
 first=ctx.refreshAppFields(2);ctx.refreshAppFields(3);resolveRead(2);await first;await settle();assert.equal(calls,3);
 resolveRead(3);await settle();assert.equal(calls,3);
 first=ctx.refreshAppFields();ctx.refreshAppFields(null,true);resolveRead(3);await first;await settle();assert.equal(calls,5);
 resolveRead(4);await settle();
 first=ctx.refreshAppFields();ctx.activeDynamicId='b';ctx.refreshAppFields();resolveRead(4);await first;await settle();assert.equal(calls,7);
 resolveRead(4);await settle();await ctx.refreshAppFields(4);assert.equal(calls,7);
 fail=true;first=ctx.refreshAppFields();resolveRead();await assert.rejects(first);await settle();assert.equal(ctx.appRead,null);assert.equal(calls,8);
 first=ctx.refreshAppFields();resolveRead(5);await first;assert.equal(calls,9);
 console.log('PASS: browser refresh coalescing, duplicate revisions, newer events, forced post-save refresh, page switches and HTTP recovery.');
})().catch(error=>{console.error(error);process.exitCode=1});
