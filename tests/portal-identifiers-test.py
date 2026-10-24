#!/usr/bin/env python3
"""Check generated DOM aliases, browser behavior and protocol-name preservation."""
import itertools
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'src/tools'))
from generate_portal_assets import portal_parts
from control_assets import selected
from portal_identifiers import identifier_maps, html_names, css_names, script_names

source = (ROOT/'src/PortalPage.h').read_text()
classes, ids = identifier_maps(source)
assert len(set(classes.values())) == len(classes)
assert len(set(ids.values())) == len(ids)
assert all(len(value) == 1 for value in [*classes.values(), *ids.values()])
assert html_names('<label for="wifiForm" class="card secondary" aria-labelledby="deviceTitle languageLabel"></label>',classes,ids) == '<label for="'+ids['wifiForm']+'" class="'+classes['card']+' '+classes['secondary']+'" aria-labelledby="'+ids['deviceTitle']+' '+ids['languageLabel']+'"></label>'
assert '#'+ids['notice']+'.'+classes['message']+'{' in css_names('#notice.message{color:#abcdef;content:".message #notice"}',classes,ids)
assert 'color:#abcdef;content:".message #notice"' in css_names('#notice.message{color:#abcdef;content:".message #notice"}',classes,ids)

scripts = []
for flags in itertools.product((False,True),repeat=5):
    options = dict(zip(('ota','mqtt','console','tls','messages'),flags))
    _, script, html = portal_parts(ROOT/'src',guarded=True,**options)
    # Different control sets exercise complete and selectively assembled assets.
    for controls in (None, [], ['text'], ['slider','text','select'], ['light','cover']):
        for zigbee, dependencies in itertools.product((0,1),repeat=2):
            compiled = selected(script,controls,{'ZIGBEE':zigbee,'DEPENDENCIES':dependencies})
            scripts.append(compiled)
    assert 'id="deviceTitle"' not in html and 'class="secondary"' not in html

_, compiled, _ = portal_parts(ROOT/'src',guarded=True)
compiled = selected(compiled,None,{'ZIGBEE':1})
fixture = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const input=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
for(const source of input.scripts)new vm.Script(source);
const c=input.classes,ids=input.ids,lines=input.compiled.split('\n');
const node=()=>({hidden:false,disabled:false,attributes:{},children:[],dataset:{},elements:{},setAttribute(k,v){this.attributes[k]=v},append(n){this.children.push(n)}});
const nodes={};for(const id of Object.values(ids))nodes[id]=node();
nodes[ids.publishForm].elements.topic={};nodes[ids.publishForm].dataset={};
const band={disabled:true,value:''},form={elements:{apBand:band}};
const requests=[];
const context=vm.createContext({document:{querySelectorAll:()=>[form],querySelector:s=>{
 if(s==='[name=apBand]')return band;
 assert.match(s,/^#[A-Za-z_][A-Za-z0-9_]*$/,'selectors must not need numeric-ID escaping');
 assert(nodes[s.slice(1)],'Unknown selector '+s);return nodes[s.slice(1)];
},getElementById:id=>{assert(nodes[id],'Unknown DOM ID '+id);return nodes[id]}},
request:async()=>({json:async()=>({deviceName:'Test',apName:'Test',mqttName:'test',apDualBand:true,apBand:5})}),
t:()=>'',setCaVisibility(){},dynamicElement:(tag,parent)=>{const n=node();parent.append(n);return n},
zigbeeBusy:false,confirm:()=>true,fetch:async path=>{requests.push(path);return {ok:true,text:async()=>'',json:async()=>({})}},zigbeeMessage(){},renderZigbee(){}});
for(const line of lines)if(line.startsWith('const _domIds=')||line.startsWith('const _pageId='))vm.runInContext(line,context);
context.dynamicDefinitions=Array.from({length:16},(_,i)=>({id:'user-page-'+i}));
const aliases=context.dynamicDefinitions.map(page=>{context.page=page;return vm.runInContext('_pageId(page)',context)});
assert.equal(new Set(aliases).size,16);assert(aliases.every(id=>id.length===1&&!Object.values(ids).includes(id)));
assert.equal(context.dynamicDefinitions[0].id,'user-page-0');
assert.equal(vm.runInContext('_domId("toString")',context),'toString');
assert.equal(vm.runInContext('_domId("dynamic-user-page")',context),'dynamic-user-page');
for(const name of ['setMessage','styleToggle','selectConsoleTab','loadConfig','operateZigbee']){
 const prefix=(name==='loadConfig'||name==='operateZigbee'?'async ':'')+'function '+name+'(';
 const line=lines.find(l=>l.startsWith(prefix));assert(line,name);vm.runInContext(line,context);
}
const message=node();context.message=message;vm.runInContext('setMessage(message,"oops",true)',context);
assert.equal(message.className,c.message+' '+c.messageError);assert.equal(message.attributes.role,'alert');
const parent=node(),toggle=node();context.parent=parent;context.toggle=toggle;
vm.runInContext('styleToggle(toggle,parent)',context);
assert.equal(parent.children[0].className,c.toggleSwitch);assert.equal(parent.children[0].children[1].className,c.toggleTrack);
vm.runInContext('selectConsoleTab("messages")',context);
for(const channel of ['mqtt','messages','zigbee']){
 assert.equal(nodes[ids[channel+'Panel']].hidden,channel!=='messages');
 assert.equal(nodes[ids[channel+'Tab']].attributes['aria-selected'],String(channel==='messages'));
}
context.currentPage={id:'wifi'};nodes[ids.wifi].hidden=true;
vm.runInContext(lines.find(l=>l.startsWith('if(currentPage)document.getElementById')),context);
assert.equal(nodes[ids.wifi].hidden,false);
(async()=>{
 await vm.runInContext('loadConfig()',context);assert.equal(band.value,5);assert.equal(band.disabled,false);
 assert.equal(nodes[ids.deviceTitle].textContent,'Test');
 await vm.runInContext('operateZigbee("reset")',context);
 assert(requests.includes('/api/zigbee/reset'));assert(requests.includes('/api/zigbee/status'));
 console.log('PASS: all generated JS variants parse; short classes/IDs, ARIA, dynamic widgets, Console tabs, configuration forms and unchanged Zigbee commands');
})().catch(e=>{console.error(e);process.exit(1)});
'''
with tempfile.TemporaryDirectory(prefix='ardportal-dom-') as directory:
    work=Path(directory)
    (work/'input.json').write_text(json.dumps({'scripts':scripts,'compiled':compiled,'classes':classes,'ids':ids}))
    (work/'test.cjs').write_text(fixture)
    subprocess.run([os.environ.get('NODE','node'),str(work/'test.cjs'),str(work/'input.json')],check=True)
