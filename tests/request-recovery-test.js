// Exercise the real serialized request helper during transient AP transport failures.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('src/PortalPage.h','utf8');
const start=source.indexOf('async function request('),end=source.indexOf('\n',start);
let calls=[],responses=[],delays=[];
const ctx=vm.createContext({languageCode:'en',t:key=>key,setTimeout:(cb,ms)=>{delays.push(ms);cb()},fetch:async(url,options)=>{calls.push(url);const next=responses.shift();if(next instanceof Error)throw next;return next}});
vm.runInContext('let requestTail=Promise.resolve();'+source.slice(start,end),ctx);
const ok=()=>({ok:true,json:async()=>({ready:true}),text:async()=>'saved'});
(async()=>{
 responses=[Error('connection reset'),ok(),ok()];
 const first=ctx.request('/api/config'),second=ctx.request('/api/status');
 assert.equal((await(await first).json()).ready,true);
 await(await second).json();
 assert.deepEqual(calls,['/api/config','/api/config','/api/status']);
 assert.deepEqual(delays,[250]);
 responses=[Error('offline'),Error('offline')];
 await assert.rejects(ctx.request('/api/status'),e=>e.network&&e.message==='ui_149');
 responses=[Error('lost reply')];const before=calls.length;
 await assert.rejects(ctx.request('/api/config',{method:'POST'}),e=>e.network);
 assert.equal(calls.length,before+1);
 responses=[{ok:false,status:503,text:async()=>'busy'}];
 await assert.rejects(ctx.request('/api/status'),e=>e.status===503);
 responses=[ok()];await(await ctx.request('/api/status')).json();
 console.log('PASS: transient GET recovery, serialized reads, persistent failure, no POST/HTTP retries and queue recovery.');
})().catch(error=>{console.error(error);process.exitCode=1});
