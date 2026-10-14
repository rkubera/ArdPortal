// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
// Run with ZHC_NODE_MODULES pointing to an isolated zigbee-herdsman-converters installation.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const root=path.resolve(import.meta.dirname,'..');
const source=fs.readFileSync(path.join(root,'src/ZigbeeConverterAssets.h'),'utf8');
const tail=source.split('R"JS(')[1].split(')JS"')[0];
const modules=process.env.ZHC_NODE_MODULES;
assert.ok(modules,'Set ZHC_NODE_MODULES to the test dependency directory');
const destination=path.join(path.dirname(modules),'ardportal-generated-test.mjs');
const fields=[
  {kind:'switch',ep:2,endpoint:'relay_ep2',label:'Relay'},
  {kind:'select',ep:3,endpoint:'mode_ep3',label:'Mode',options:['off','eco','comfort']},
  {kind:'state',ep:4,endpoint:'state_ep4',label:'State',options:['not_home','home']},
  {kind:'slider',ep:5,endpoint:'setpoint_ep5',label:'Setpoint',min:-10,max:80,step:0.25},
  {kind:'climate',ep:6,endpoint:'climate_ep6',label:'Climate',min:5,max:45,step:0.25,modes:['off','heat','cool','auto'],fanModes:['off','low','high','auto']},
  {kind:'cover',ep:7,endpoint:'blind_ep7',label:'Blind'},
  {kind:'light',ep:8,endpoint:'rgb_ep8',label:'RGB'},
  {kind:'action',ep:9,endpoint:'button_ep9',label:'Button'},
  {kind:'lock',ep:10,endpoint:'lock_ep10',label:'Lock'},
  {kind:'siren',ep:11,endpoint:'siren_ep11',label:'Siren'},
  ...['number','temperature','humidity','pressure','illuminance','flow','carbon_dioxide','pm25','wind_speed','voltage','current','power','binary'].map((kind,index)=>({kind,ep:index+12,endpoint:kind+'_ep'+(index+12),label:kind})),
  {kind:'action',ep:40,endpoint:'vacuum_ep40',label:'Vacuum / start'},
  {kind:'action',ep:41,endpoint:'vacuum_ep41',label:'Vacuum / stop'},
  {kind:'action',ep:42,endpoint:'mower_ep42',label:'Mower / start'},
  {kind:'action',ep:43,endpoint:'mower_ep43',label:'Mower / dock'},
];
fs.writeFileSync(destination,"import * as m from 'zigbee-herdsman-converters/lib/modernExtend';\nimport {presets as e, access as a} from 'zigbee-herdsman-converters/lib/exposes';\nconst identity={model:'ArdPortal-0123456789abcdef'};\nconst fields="+JSON.stringify(fields).slice(0,-1)+tail);
try{
  const {default:definition}=await import(pathToFileURL(destination));
  assert.deepEqual(definition.fingerprint,[{modelID:'ArdPortal-0123456789abcdef',manufacturerName:'ArdPortal'}]);
  assert.equal(definition.extend.length,fields.length-1);
  const exposes=definition.extend.flatMap(x=>x.exposes??[]);
  const at=(x,endpoint)=>x.property?.endsWith('_'+endpoint)||x.features?.some(f=>at(f,endpoint));
  assert.ok(exposes.some(x=>x.type==='climate'&&at(x,'climate_ep6')));
  assert.ok(exposes.some(x=>x.type==='enum'&&at(x,'mode_ep3')&&x.values.includes('comfort')));
  assert.ok(exposes.some(x=>x.type==='numeric'&&at(x,'wind_speed_ep20')));
  assert.ok(!definition.extend.some(x=>x.meta?.multiEndpointSkip?.includes('state')));
  const relay=exposes.find(x=>x.type==='switch');assert.equal(relay.label,'Relay');assert.equal(relay.features[0].label,'Relay');
  assert.equal(exposes.find(x=>at(x,'mode_ep3')).label,'Mode');
  const blind=exposes.find(x=>x.type==='cover');assert.ok(at(blind,'blind_ep7'));
  const climate=exposes.find(x=>x.type==='climate');assert.equal(climate.label,'Climate');assert.ok(climate.features.every(feature=>feature.label.startsWith('Climate / ')));
  assert.equal(exposes.filter(x=>x.type==='climate').length,1);
  const setpoint=climate.features.find(x=>x.name==='occupied_heating_setpoint');
  assert.equal(setpoint.value_min,5);assert.equal(setpoint.value_max,45);assert.equal(setpoint.value_step,0.25);
  const select=definition.extend[2],writes=[];
  const entity={write:async(...args)=>writes.push(args),command:async(...args)=>writes.push(args)};
  const meta={endpoint_name:'mode_ep3',device:{},mapped:{meta:{}}};
  await select.toZigbee[0].convertSet(entity,'value','comfort',meta);
  assert.deepEqual(writes[0].slice(0,2),['genMultistateOutput',{85:{value:3,type:33}}]);
  assert.equal(select.fromZigbee[0].convert({endpoint:()=>({mode_ep3:3}),meta:{multiEndpoint:true}},
    {endpoint:{ID:3},data:{85:2}},null,null,{device:{}}).value_mode_ep3,'eco');
  const action=definition.extend[8];await action.toZigbee[0].convertSet(entity,'activate','PRESS',meta);
  assert.equal(writes.at(-1)[1],'on');
  const grouped=exposes.find(x=>at(x,'vacuum_ep40'));
  assert.deepEqual(grouped.values,['start','stop']);assert.equal(grouped.label,'Vacuum');
  assert.equal(grouped.access,2);assert.ok(!exposes.some(x=>at(x,'vacuum_ep41')));
  const groupConverter=definition.extend.flatMap(x=>x.toZigbee??[]).find(x=>x.endpoints?.includes('vacuum_ep40'));
  const calls=[];const groupMeta={device:{getEndpoint:ep=>({command:async(...args)=>calls.push([ep,...args])})}};
  for(const value of ['start','start','stop']){
    const result=await groupConverter.convertSet(entity,'activate',value,groupMeta);
    assert.deepEqual(result,{state:{activate:null}});
  }
  assert.deepEqual(calls.map(x=>x[0]),[40,40,41]);
  assert.ok(calls.every(x=>x[1]==='genOnOff'&&x[2]==='on'));
  await assert.rejects(groupConverter.convertSet(entity,'activate','invalid',groupMeta));
  assert.equal(calls.length,3);
  assert.deepEqual(exposes.find(x=>at(x,'mower_ep42')).values,['start','dock']);
  const hidden=x=>!x.endpoint&&(x.features??[]).every(hidden);
  assert.ok(exposes.every(hidden));
  assert.equal(relay.features[0].property,'state_relay_ep2');
  assert.ok(definition.extend[1].toZigbee.every(c=>c.endpoints.includes('relay_ep2')));
  console.log('PASS: actual ZHC API, named options, both directions, layout fingerprint, climate ranges and isolated endpoints');
}finally{fs.unlinkSync(destination);}
