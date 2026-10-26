const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(__dirname+'/../src/PortalPage.h','utf8');
const start=source.indexOf('let matterBusy=');
const script=source.slice(start,source.indexOf('/*@endif*/',start));
const nodes=new Map(),calls=[];
function node(id){if(!nodes.has(id))nodes.set(id,{disabled:false,textContent:'',replaceChildren(){},append(){}});return nodes.get(id)}
let accept=false,failure=false,qrUrl='';
const pixels=[];node('#matterQr').getContext=()=>({fillStyle:'',fillRect:(...args)=>pixels.push(args)});
const context=vm.createContext({document:{querySelector:node,getElementById:id=>node('#'+id),createElement:()=>({})},currentPage:null,languageCode:'',t:key=>key,setInterval(){},confirm:()=>accept,setMessage:(n,text,error)=>{n.textContent=text;n.error=!!error},request:async(url,options)=>{calls.push({url,options});if(failure)throw Error('network reserve');return {json:async()=>url==='/api/matter/qr'?{size:21,bits:'f'.repeat(111)}:({qrUrl,state:'commissioning',transport:'wifi',endpoints:3,unsupported:0,manualCode:'123',error:''}),text:async()=>'accepted'}}});
vm.runInContext(script,context);
(async()=>{
 await vm.runInContext('refreshMatter()',context);assert.equal(calls.length,0,'No status/QR requests before the language dictionary loads');
 context.languageCode='en';
 await vm.runInContext('refreshMatter()',context);
 assert.equal(node('#rejoin').disabled,false);
 assert.match(node('#matterCode').textContent,/123/);
 await vm.runInContext("renderMatter({credentials:'production',state:'commissioning',endpoints:3,unsupported:0,manualCode:''})",context);assert.equal(node('#matterCode').textContent,'ui_230');
 qrUrl='https://example.test/?data=MT%3ATEST';await vm.runInContext('refreshMatter()',context);assert.equal(node('#matterQr').hidden,false);assert.equal(node('#matterQr').width,174);assert.equal(pixels.length,442);const qrCalls=calls.filter(c=>c.url==='/api/matter/qr').length;await vm.runInContext('refreshMatter()',context);assert.equal(calls.filter(c=>c.url==='/api/matter/qr').length,qrCalls);
 calls.length=0;await vm.runInContext("operateMatter('reset')",context);assert.equal(calls.length,0);
 accept=true;await vm.runInContext("operateMatter('reset')",context);assert.equal(calls[0].options.body,'confirm=RESET-MATTER');assert.equal(calls[1].url,'/api/matter/status');
 await vm.runInContext("renderMatter({state:'no_endpoints',endpoints:0,unsupported:1,error:'',info:'No Matter controls'})",context);assert.equal(node('#rejoin').disabled,true);assert.equal(node('#reset').disabled,true);assert.equal(node('#matter #result').textContent,'No Matter controls');assert.equal(node('#matter #result').error,false);
 await vm.runInContext("renderMatter({state:'commissioning',endpoints:1,error:''})",context);assert.equal(node('#matter #result').textContent,'');assert.equal(node('#rejoin').disabled,false);
 failure=true;await vm.runInContext('refreshMatter()',context);assert.equal(node('#matter #result').textContent,'network reserve');
 assert.ok(source.includes("if(currentPage?.id==='matter')await refreshMatter()"));
 assert.ok(source.includes("id:'matter',path:'/matter',label:'ui_221'"));
 console.log('PASS: Matter menu, polling, reset confirmation and network errors');
})().catch(error=>{console.error(error);process.exitCode=1});
