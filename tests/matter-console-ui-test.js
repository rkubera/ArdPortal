const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(__dirname+'/../src/PortalPage.h','utf8');
const nodes=new Map(),sockets=[];
function node(id){if(!nodes.has(id))nodes.set(id,{hidden:false,disabled:false,textContent:'',scrollHeight:0,scrollTop:0,clientHeight:0,setAttribute(k,v){this[k]=v}});return nodes.get(id)}
class Socket {constructor(url){this.url=url;this.readyState=0;sockets.push(this)}close(){this.readyState=3}}
const context=vm.createContext({document:{querySelector:node},location:{protocol:'http:',host:'device.local'},currentPage:{id:'console'},
 consoleRows:{mqtt:[],messages:[],matter:[],zigbee:[]},consoleSocket:null,consoleConnected:false,consoleMqtt:false,
 consoleErrorKey:'',consoleRetry:null,socketLastSeen:0,restartPending:false,languageCode:'',languageLocale:'en',
 matterConsoleSocket:null,matterConsoleRetry:null,matterConsoleLastSeen:0,t:key=>key,WebSocket:Socket,setMessage(){},refreshAppFields:async()=>{},setTimeout(){},Date});
for(const name of ['appendConsoleLine','selectConsoleTab','formatConsoleLine','consoleAvailability','openConsole','openMatterConsole'])vm.runInContext(source.split('\n').find(line=>line.startsWith('function '+name+'(')),context);
vm.runInContext('openMatterConsole()',context);assert.equal(sockets.length,0);context.languageCode='en';
vm.runInContext("selectConsoleTab('matter');openMatterConsole();openMatterConsole();openConsole()",context);
assert.equal(node('#matterPanel').hidden,false);assert.equal(node('#messagesPanel').hidden,true);assert.equal(node('#mqttPanel').hidden,true);
assert.equal(sockets.length,2);assert.equal(sockets[0].url,'ws://device.local/api/matter/console');assert.equal(sockets[1].url,'ws://device.local/api/console');sockets[0].onopen();
sockets[0].onmessage({data:JSON.stringify({type:'status',connected:true})});assert.ok(context.matterConsoleLastSeen>0);assert.equal(context.consoleRows.matter.length,0);
const record={type:'log',channel:'matter',id:1,text:'Received: Desk socket — Turn on — Accepted'};
sockets[0].onmessage({data:JSON.stringify(record)});sockets[0].onmessage({data:JSON.stringify(record)});
assert.equal(context.consoleRows.matter.length,1);assert.equal(context.consoleRows.messages.length,0);assert.equal(node('#matterLog').textContent,record.text);
assert.ok(source.includes("s.matterCommissioned?(s.matterConnected?'ui_078':'ui_079'):'ui_234'"));
console.log('PASS: Matter tab selection, independent read-only socket, readable traffic routing, deduplication and main status states');
