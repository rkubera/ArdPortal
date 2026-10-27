// SPDX-License-Identifier: MIT
// Run the shipped navigation, tab rendering and visibility refresh against a small DOM.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../src/PortalPage.h'),'utf8');
function selected(text,dependencies){
 const flags=Object.fromEntries([...text.matchAll(/ARDPORTAL_[A-Z_]+/g)].map(m=>[m[0],0]));
 Object.assign(flags,{ARDPORTAL_ENABLE_GROUPS:1,ARDPORTAL_ENABLE_DYNAMIC_PAGES:1,ARDPORTAL_ENABLE_DEPENDENCIES:dependencies});
 const stack=[true];let cursor=0,result='';
 for(const m of text.matchAll(/\/\*@(?:(if) ([^*]+)|(endif))\*\//g)){
  if(stack[stack.length-1])result+=text.slice(cursor,m.index);
  if(m[1])stack.push(stack[stack.length-1]&&!!vm.runInNewContext(m[2],flags));else{assert(stack.length>1);stack.pop()}
  cursor=m.index+m[0].length;
 }
 assert.equal(stack.length,1);return result+(stack[0]?text.slice(cursor):'');
}
class Node{
 constructor(tag='div'){this.tagName=tag;this.children=[];this.dataset={};this.attributes={};this.hidden=false;this.style={}}
 append(...children){for(const child of children){child.remove();child.parentNode=this;this.children.push(child)}}
 remove(){if(this.parentNode){const list=this.parentNode.children;list.splice(list.indexOf(this),1);this.parentNode=null}}
 after(child){const parent=this.parentNode;child.remove();child.parentNode=parent;parent.children.splice(parent.children.indexOf(this)+1,0,child)}
 replaceChildren(...children){for(const child of [...this.children])child.remove();this.append(...children)}
 contains(node){return node===this||this.children.some(child=>child.contains(node))}
 setAttribute(key,value){this.attributes[key]=String(value)}
}
async function exercise(dependencies,route='/p/last'){
 const header=new Node('header'),main=new Node('main'),nodes={};
 for(const id of ['navigation','dynamicNavigation','home','info','dynamicLoading']){const node=nodes[id]=new Node(id.includes('Navigation')||id==='navigation'?'nav':'div');node.id=id;(id==='navigation'||id==='dynamicNavigation'?header:main).append(node)}
 const all=()=>{const visit=n=>[n,...n.children.flatMap(visit)];return [...visit(header),...visit(main)]};
 const groups=[{id:'g',name:'Group',names:{pl:'Grupa'},order:10},{id:'empty',name:'Empty',order:-1}];
 let visibility={},pendingAppRead=null,resolveAppRead;
 const conditionalFields=Array.from({length:192},(_,i)=>({id:'dependent_'+i,type:'text',name:'Dependent '+i,visibleWhen:{field:'controller',equals:i}}));
 const snapshot={revision:1,values:{controller:0},conditions:Object.fromEntries(conditionalFields.map((f,i)=>[f.id,{visible:i===0}]))};
 const ctx=vm.createContext({document:{title:'Device',createElement:tag=>new Node(tag),getElementById:id=>all().find(n=>n.id===id),querySelector:s=>s==='main'?main:nodes[s.slice(1)]},
  location:{pathname:route},pages:[{id:'wifi',path:'/wifi',label:'Wi-Fi'}],nav:nodes.navigation,
  dynamicDefinitions:[],dynamicBindings:[],dynamicLabels:[],dynamicCatalogCount:0,activeDynamicId:'',currentPage:null,languageCode:'en',
  t:key=>key,applyTranslations(){},refreshAppFields:()=>Promise.resolve(),
  request:async path=>{assert(['/api/groups','/api/page-conditions','/api/app?page=last'].includes(path),path);return{json:async()=>path.startsWith('/api/app?')?(pendingAppRead||snapshot):path==='/api/groups'?JSON.parse(JSON.stringify(groups)):visibility}},
  notice:{textContent:''},appError:'',showNotice(){},appSnapshotPage:'',appSnapshotRevision:null});
 const run=text=>vm.runInContext(text,ctx);
 for(const name of ['link','localized','dynamicLabel','translateDynamic','dynamicElement','isHomePage','activeDynamicPage']){
  const line=source.split('\n').find(l=>l.startsWith('function '+name+'('));assert(line,name);run(selected(line,dependencies));
 }
 const applyStart=source.indexOf('function applyDynamicVisibility('),applyEnd=source.indexOf('/** @brief Preview only dependencies',applyStart);
 run(source.slice(applyStart,applyEnd));
 const start=source.indexOf('let pageGroups='),end=source.indexOf('\n/*@endif*/\nlet dynamicCatalogRead=',start);
 assert(start>=0&&end>start);run(selected(source.slice(start,end),dependencies));
 const loadStart=source.indexOf('async function loadDynamicPages('),loadEnd=source.indexOf('\n/*@endif*/\nlet portalReady=',loadStart);
 run(selected(source.slice(loadStart,loadEnd),dependencies));
 const read=source.split('\n').find(l=>l.startsWith('async function readAppFields('));run(selected(read,dependencies));
 const catalog=[{id:'last',group:'g',order:10,name:'Home',names:{pl:'Ostatnia'},fields:dependencies?[{id:'controller',type:'text',name:'Controller'},...conditionalFields]:[]},
  {id:'first',group:'g',order:1,name:'First',names:{pl:'Pierwsza'},fields:[],...(dependencies?{visibleWhen:{field:'switch',equals:true}}:{})},
  {id:'tie',group:'g',order:1,name:'Tie',fields:[]},{id:'outside',order:0,name:'Outside',fields:[]},
  {id:'lost',group:'unregistered',order:0,name:'Lost',fields:[]}];
 await ctx.loadDynamicPages(catalog);
 const group=run('pageGroups[0]'),empty=run('pageGroups[1]'),tabs=run('groupTabs');
 assert.equal(group.pages.map(p=>p.id).join(','),'first,tie,last');
 assert.equal(run('pageGroup({id:"missing"},5).order'),5);
 assert.equal(run('pageGroup({id:"missing",order:-2},5).order'),-2);
 assert.equal(group.menuLink.href,dependencies?'/p/tie':'/p/first');
 assert.equal(empty.menuLink.hidden,true);
 assert.deepEqual(nodes.dynamicNavigation.children.filter(a=>!a.hidden).map(a=>a.textContent),['Outside','Group']);
 assert(!nodes.dynamicNavigation.children.some(a=>a.href==='/p/lost'));
 assert(!Object.hasOwn(group.menuLink.dataset,'i18n'));
 if(route==='/p/lost'){
  assert.equal(tabs.hidden,true);assert.equal(tabs.children.length,0);return;
 }
 assert.equal(tabs.attributes.role,'tablist');
 assert.deepEqual(tabs.children.map(a=>a.href),['/p/first','/p/tie','/p/last']);
 assert.deepEqual(tabs.children.map(a=>a.attributes['aria-selected']),['false','false','true']);
 assert.equal(tabs.children[0].hidden,!!dependencies);
 assert.equal(tabs.children[2].attributes['aria-current'],'page');
 assert.equal(group.menuLink.attributes['aria-current'],'page');
 assert.equal(run('isHomePage(dynamicDefinitions[0])'),false);
 ctx.languageCode='pl';ctx.translateDynamic();
 assert.equal(group.menuLink.textContent,'Grupa');assert.equal(tabs.children[0].textContent,'Pierwsza');
 if(dependencies){
  // Render the real large form, then delay the first app snapshot after page visibility arrives.
  assert.equal(ctx.dynamicBindings.length,193);
  assert.equal(ctx.dynamicBindings[0].container.hidden,false);
  assert(ctx.dynamicBindings.slice(1).every(b=>b.container.hidden&&b.enabled===false));
  pendingAppRead=new Promise(resolve=>resolveAppRead=resolve);
  const firstRead=ctx.readAppFields();await new Promise(setImmediate);
  assert(ctx.dynamicBindings.slice(1).every(b=>b.container.hidden));
  resolveAppRead(snapshot);await firstRead;pendingAppRead=null;
  assert.equal(ctx.dynamicBindings[1].container.hidden,false);
  assert(ctx.dynamicBindings.slice(2).every(b=>b.container.hidden));
  visibility=Object.fromEntries(catalog.map(p=>[p.id,false]));await ctx.readAppFields();
  assert.equal(group.menuLink.hidden,true);assert.equal(nodes.dynamicNavigation.hidden,true);assert.equal(tabs.hidden,true);
  visibility.last=true;await ctx.readAppFields();
  assert.equal(group.menuLink.href,'/p/last');assert.equal(group.menuLink.hidden,false);assert.equal(nodes.dynamicNavigation.hidden,false);assert.equal(tabs.hidden,false);
  assert.deepEqual(tabs.children.map(a=>a.hidden),[true,true,false]);
 }
 // Catalog refresh must rebuild membership and preserve the single tab bar and label bindings.
 const late={id:'late',group:'g',order:2,name:'Late',fields:[]};
 await ctx.loadDynamicPages([...catalog,late]);
 assert.strictEqual(run('groupTabs'),tabs);
 assert.deepEqual(tabs.children.map(a=>a.href),['/p/first','/p/tie','/p/late','/p/last']);
 const labelCount=ctx.dynamicLabels.length;
 await ctx.loadDynamicPages([...catalog,late]);
 assert.equal(ctx.dynamicLabels.length,labelCount);
 assert.equal(header.children.filter(n=>n.attributes.role==='tablist').length,1);
}
(async()=>{await exercise(0);await exercise(1);await exercise(1,'/p/lost');console.log('PASS: 192 conditional controls stay collapsed before a delayed initial snapshot; group navigation, visibility and catalog refresh with dependencies on/off')})().catch(error=>{console.error(error);process.exitCode=1});
