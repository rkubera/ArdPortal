// Exercise the real configuration loader with dual-band and single-band devices.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('src/PortalPage.h','utf8');
let config={apDualBand:true,apBand:5,deviceName:'Test',apName:'Test',mqttName:'test'};
const band={value:'',disabled:true},choice={hidden:true},form={elements:{apBand:band}};
const nodes={'#apBandChoice':choice,'[name=apBand]':band,'#deviceTitle':{},'#apSsid':{},'#publishForm':{dataset:{},elements:{topic:{}}}};
const context=vm.createContext({document:{querySelectorAll:()=>[form],querySelector:s=>nodes[s]},request:async()=>({json:async()=>config}),t:()=>'',setCaVisibility(){}});
vm.runInContext(source.split('\n').find(l=>l.startsWith('async function loadConfig(')),context);
(async()=>{await vm.runInContext('loadConfig()',context);assert.equal(choice.hidden,false);assert.equal(band.disabled,false);assert.equal(band.value,5);config={...config,apDualBand:false,apBand:24};await vm.runInContext('loadConfig()',context);assert.equal(choice.hidden,true);assert.equal(band.disabled,true);console.log('PASS: AP band shown only on capable devices; selected value restored');})().catch(e=>{console.error(e);process.exit(1);});
