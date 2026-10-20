// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
// Called by zigbee-converter-test.mjs when Z2M_HA_SOURCE points to upstream lib/extension/homeassistant.ts.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {stripTypeScriptTypes} from 'node:module';

/** Execute the actual gateway Discovery mapper against exported exposes. @param exposes Generated ZHC exposes. @returns No value; asserts unique IDs and explicit names. */
export function checkHomeAssistantDiscovery(exposes){
  const source=fs.readFileSync(process.env.Z2M_HA_SOURCE,'utf8');
  const constants=source.slice(source.indexOf('const ACTION_PATTERNS'),source.indexOf('class Bridge'));
  const start=source.indexOf('    private exposeToConfig(');
  const end=source.indexOf('\n    @bind async onEntityRemoved',start);
  assert.ok(start>=0&&end>start,'Expected upstream Discovery mapper');
  const code=stripTypeScriptTypes(constants+'\nclass Mapper {\n'+source.slice(start,end).replaceAll('<DiscoveryEntry>','')+'\n}\n');
  const isType=type=>expose=>expose.type===type;
  const isNumericExpose=isType('numeric'),isBinaryExpose=isType('binary'),isEnumExpose=isType('enum');
  const assertType=predicate=>expose=>assert.ok(predicate(expose));
  const utils={capitalize:text=>text.charAt(0).toUpperCase()+text.slice(1),arrayUnique:items=>[...new Set(items)],flatten:items=>items.flat()};
  const Mapper=new Function('assert','utils','isNumericExpose','isBinaryExpose','isEnumExpose',
    'assertNumericExpose','assertBinaryExpose','assertEnumExpose',code+'\nreturn Mapper;')(
      assert,utils,isNumericExpose,isBinaryExpose,isEnumExpose,
      assertType(isNumericExpose),assertType(isBinaryExpose),assertType(isEnumExpose));
  const mapper=new Mapper();
  const entity={isDevice:()=>true,isGroup:()=>false,endpoint:name=>({ID:name})};
  const ids=new Set();
  for(const expose of exposes){
    const entries=mapper.exposeToConfig([expose],entity,exposes);
    for(const entry of entries){
      const key=entry.type+'/'+entry.object_id;
      assert.ok(!ids.has(key),'Discovery collision: '+key);ids.add(key);
      assert.equal(entry.discovery_payload.name,expose.label,'Missing registered label for '+key);
    }
  }
  console.log('PASS: actual Zigbee2MQTT Home Assistant Discovery mapper, unique entity IDs and registered names');
}
