// SPDX-License-Identifier: MIT
// Exercise the actual navigation comparator, including stable equal-order links.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../src/PortalPage.h'),'utf8');
const match=source.match(/\.sort\((\(a,b\)=>a\.order-b\.order)\)/);
assert(match,'Navigation order comparator missing');
const compare=vm.runInNewContext('('+match[1]+')');
const pages=[{id:'first',order:10},{id:'second',order:1},{id:'third',order:1},
 {id:'fourth',order:3},{id:'fifth',order:-1},{id:'sixth',order:5}];
assert.deepStrictEqual([...pages].sort(compare).map(p=>p.id),['fifth','second','third','fourth','sixth','first']);
assert.deepStrictEqual([...pages].map(p=>({...p,order:0})).sort(compare).map(p=>p.id),pages.map(p=>p.id));
console.log('PASS: navigation order, negative/explicit/default positions and stable registration-order ties');
