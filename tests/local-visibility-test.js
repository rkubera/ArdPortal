// Run the shipped visibility evaluator and control-queue hook without HTTP latency.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('src/PortalPage.h','utf8');
const start=source.indexOf('let appPreviewValues='),end=source.indexOf('/**\n * @brief Queue a changed control value',start);
const leaf=(field,equals,property)=>property===undefined?{field,equals}:{field,equals,property};
const fields=[{id:'mode'},{id:'enabled'},{id:'object'},
 {id:'manual',visibleWhen:leaf('mode','manual')},
 {id:'child',visibleWhen:leaf('manual','yes')},
 {id:'property',visibleWhen:leaf('object',2,'level')},
 {id:'both',visibleWhen:{and:[leaf('mode','manual'),leaf('enabled',true)]}},
 {id:'either',visibleWhen:{or:[leaf('mode','manual'),leaf('enabled',true)]}},
 {id:'external',visibleWhen:leaf('other-page','manual')},
 {id:'cycle',visibleWhen:leaf('cycle',true)}];
const binding=field=>({field,container:{hidden:true},node:{disabled:true},write(value){this.lastWritten=value},enabled:true,read(){return 'manual'}});
const bindings=fields.map(binding),page={id:'a',fields};let sends=0,resolveRead;
const ctx=vm.createContext({appSnapshotPage:'a',activeDynamicId:'a',appSnapshotRevision:0,dynamicDefinitions:[page],dynamicBindings:bindings,dynamicQueue:new Map(),dynamicActionId:0,
 flushDynamic(){++sends;return Promise.resolve()},request:async()=>({json:()=>new Promise(resolve=>resolveRead=resolve)}),appError:'',notice:{textContent:''},showNotice(){},t:x=>x,localized:()=>'',document:{querySelector(){return {hidden:false}}}});
vm.runInContext(source.slice(start,end),ctx);
const run=text=>vm.runInContext(text,ctx);
run('appPreviewValues={mode:"auto",enabled:false,object:{level:2},manual:"yes",cycle:true};previewDynamicVisibility()');
assert.equal(bindings[3].container.hidden,true);assert.equal(bindings[4].container.hidden,true);assert.equal(bindings[5].container.hidden,false);
const qstart=source.indexOf('function queueDynamic('),qend=source.indexOf('\n/**',qstart);
vm.runInContext(source.slice(qstart,qend),ctx);
ctx.queueDynamic(bindings[0],page,undefined,'manual');
assert.equal(sends,1);assert.equal(bindings[3].container.hidden,false);assert.equal(bindings[4].container.hidden,false);assert.equal(bindings[6].container.hidden,true);assert.equal(bindings[7].container.hidden,false);
assert.equal(bindings[8].container.hidden,true);assert.equal(bindings[9].container.hidden,true);
run('appPreviewValues.object={level:"2"};previewDynamicVisibility()');assert.equal(bindings[5].container.hidden,true);
page.visibleWhen=leaf('enabled',true);run('previewDynamicVisibility()');assert.equal(bindings[3].container.hidden,true);delete page.visibleWhen;
// A hidden group is shown if at least one member becomes visible.
const group={hidden:true};bindings[3].group=group;bindings[5].group=group;run('previewDynamicVisibility()');assert.equal(group.hidden,false);
// A snapshot for another page must never preview this page.
ctx.appSnapshotPage='other';bindings[3].container.hidden=true;run('previewDynamicVisibility()');assert.equal(bindings[3].container.hidden,true);ctx.appSnapshotPage='a';
// Old in-flight GETs preserve pending local intent; a fresh GET after save reconciles it.
const rstart=source.indexOf('async function readAppFields()'),rend=source.indexOf('\n',rstart);
vm.runInContext(source.slice(rstart,rend),ctx);
(async()=>{
 let read=ctx.readAppFields();await new Promise(setImmediate);
 bindings[0].dirty=false;bindings[0].saving=false;
 resolveRead({revision:1,values:{mode:'auto'},conditions:{manual:{visible:false}}});await read;
 assert.equal(bindings[3].container.hidden,false);assert.equal(bindings[0].previewValue,'manual');assert.equal(bindings[0].lastWritten,undefined);
 read=ctx.readAppFields();await new Promise(setImmediate);
 resolveRead({revision:2,values:{mode:'auto'},conditions:{manual:{visible:false}}});await read;
 assert.equal(bindings[3].container.hidden,true);assert.equal(Object.hasOwn(bindings[0],'previewValue'),false);assert.equal(bindings[0].lastWritten,'auto');
 for(let i=0;i<192;i++){const field={id:'bulk'+i,visibleWhen:leaf('mode','manual')};page.fields.push(field);bindings.push(binding(field))}
 ctx.queueDynamic(bindings[0],page,undefined,'manual');assert(bindings.slice(-192).every(b=>!b.container.hidden));
 console.log('PASS: 192 dependent fields, immediate select visibility, chains, groups, typed properties, page conditions, unknown sources, cycles, page switches and authoritative reconciliation.');
})().catch(error=>{console.error(error);process.exitCode=1});
