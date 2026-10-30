// Exercise the shipped input setup with ASCII and multibyte values.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../src/PortalPage.h'),'utf8');
const start=source.indexOf("node.type='text';node.readOnly=field.type==='text'");
assert(start>=0);
const setup=source.slice(start,source.indexOf('}binding.write=',start));
for(const [field,limit]of [[{type:'edit'},128],[{type:'edit',maxLength:1024},1024],[{type:'edit',maxLength:4},4]]){
 const node={setCustomValidity(message){this.message=message}};
 vm.runInNewContext(setup,{node,field,TextEncoder});
 assert.equal(node.maxLength,limit);
 node.value='a'.repeat(limit);node.oninput();assert.equal(node.message,'');
 node.value+='a';node.oninput();assert(node.message);
 node.value='ą'.repeat(limit/2);node.oninput();assert.equal(node.message,'');
 node.value+='ą';node.oninput();assert(node.message);
}
const node={};vm.runInNewContext(setup,{node,field:{type:'text',maxLength:1024},TextEncoder});
assert(node.readOnly);assert.equal(node.maxLength,undefined);
assert(source.includes('if(!node.reportValidity())return;return queueDynamic'));
console.log('PASS: editable UTF-8 byte limits and read-only text input');
