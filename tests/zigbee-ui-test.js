// Exercise the actual standalone Zigbee script with a controlled DOM/network.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../src/PortalPage.h'), 'utf8');
const script = source.slice(source.indexOf('let zigbeeBusy='),source.indexOf('/*@endif*/\nstartPortal();',source.indexOf('let zigbeeBusy='))); 
const nodes = new Map();
/** @brief Create/get a test element. @param id Selector. @return Element stub. */
function node(id) {
  if (!nodes.has(id)) nodes.set(id, {disabled: false, textContent: '', hidden: false, attrs: {},
    scrollHeight: 10, scrollTop: 0, clientHeight: 20,
    classList: {toggle() {}}, setAttribute(k,v) {this.attrs[k]=v;},
    replaceChildren() {}, append() {}});
  return nodes.get(id);
}
const requests = [], sockets = [];
const translations=JSON.parse(fs.readFileSync(path.join(__dirname,"../src/languages/en.json"),"utf8")).strings;
let confirmation;
let state = {state:'pairing', pairing:true, busy:true, paired:false, role:'end-device', channel:0, panId:0, attempts:1, error:''};
class Socket {
  constructor(url) {this.url=url;this.readyState=1;sockets.push(this);}
  close() {this.readyState=3;this.onclose?.();}
}
const context = vm.createContext({document:{querySelector:node, getElementById:id=>node('#'+id), createElement:()=>node(Symbol())},
  location:{protocol:'http:',host:'device.local'}, currentPage:{id:'zigbee'}, languageCode:'en', WebSocket:Socket, confirm:text=>{confirmation=text;return true;},
  window:{addEventListener(){}}, setInterval(){}, setTimeout(){}, clearTimeout(){},
  fetch:async(url,options)=>{requests.push({url,options});return {ok:true,text:async()=> 'accepted',json:async()=>state};},
  t:key=>translations[key],
  Date,JSON,Error,String});
vm.runInContext(script,context);
/** @brief Exercise switch commands and console deduplication. @return Promise resolving after assertions. */
async function test() {
  await vm.runInContext("refreshZigbee()",context); await new Promise(setImmediate);
  assert.equal(sockets.length,0);
  assert.equal(node('#zigbeeConverterDownload').hidden,true);
  state.converterReady=true;await vm.runInContext("refreshZigbee()",context);
  assert.equal(node('#zigbeeConverterDownload').hidden,false);
  assert.ok(source.includes('<button id="zigbeeConverterDownload"'));
  node('#zigbeeConverterDownload').onclick();
  assert.equal(context.location.href,'/api/zigbee/converter.mjs');
  assert.equal(node('#zhaQuirkDownload').hidden,true);
  state.zhaQuirkReady=true;await vm.runInContext("refreshZigbee()",context);
  assert.equal(node('#zhaQuirkDownload').hidden,false);
  node('#zhaQuirkDownload').onclick();assert.equal(context.location.href,'/api/zigbee/zha-quirk.py');
  requests.length=0;
  assert.ok(!source.includes('id="pair"'));
  await vm.runInContext("operateZigbee('rejoin')",context);
  assert.equal(requests.length,0); // Active automatic series blocks another operation.
  state={...state,state:'failed',pairing:false,busy:false,paired:true};
  await vm.runInContext('refreshZigbee()',context);
  await vm.runInContext("operateZigbee('rejoin')",context);
  assert.equal(requests.at(-2).url,'/api/zigbee/rejoin');
  translations.ui_220='Localized reset confirmation';
  await vm.runInContext("operateZigbee('reset')",context);
  assert.equal(confirmation,'Localized reset confirmation');
  assert.equal(requests.at(-2).url,'/api/zigbee/reset');
  assert.equal(requests.at(-2).options.body,'confirm=RESET-ZIGBEE');
  console.log('PASS: automatic pairing UI, saved-network rejoin and reset confirmation');
}
test().catch(error=>{console.error(error);process.exitCode=1;});
