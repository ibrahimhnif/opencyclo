import assert from "node:assert/strict";
import vm from "node:vm";
import {readFile} from "node:fs/promises";
const ctx={window:{}};
for(const f of ["flows.js","more-flows.js","audit.js"])vm.runInNewContext(await readFile(f,"utf8"),ctx);
const {flows,audit}=ctx.window.REVIEW;
assert.equal(flows.length,11);
assert.equal(new Set(flows.map(f=>f.id)).size,flows.length);
function target(flow,id) {
 const [a,b]=id.includes(":")?id.split(":"):[flow.id,id];
 const found=flows.find(f=>f.id===a);assert(found,"Missing flow "+a);
 assert(found.states.some(s=>s.id===b),"Missing state "+a+":"+b);
}
let states=0,edges=0;
for(const f of flows) {
 assert(f.current && f.review && f.contract && f.source);
 assert.equal(new Set(f.states.map(s=>s.id)).size,f.states.length);
 for(const s of f.states) {
  states++;assert(s.title && s.status && s.note);
  assert((s.buttons||[]).length<=2);
  if(s.busy)assert(!(s.buttons||[]).length);
  if(s.back)target(f,s.back);
  for(const b of [...(s.buttons||[]),...(s.events||[])]){target(f,b[1]);edges++;}
 }
}
for(const a of audit){assert(flows.some(f=>f.id===a.flow));assert(a.evidence && a.acceptance);}
// Minimal-DOM controller test; not a browser/layout test.
class Element {
 constructor(){this.innerHTML="";this.textContent="";this.hidden=false;this.dataset={};this.className="";this.classList={toggle(){}};}
 setAttribute(){}
}
const elements=new Map();
const get=id=>{if(!elements.has(id))elements.set(id,new Element());return elements.get(id);};
let listener;
ctx.document={getElementById:get,querySelector:()=>get("caption"),querySelectorAll:()=>[],addEventListener:(type,fn)=>{listener=fn;}};
vm.runInNewContext(await readFile("review.js","utf8"),ctx);
const app=ctx.window.reviewApp;
for(const f of flows)for(const s of f.states){
 app.selectFlow(f.id);app.go(s.id);
 const escaped=s.title.replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
 assert(get("screen").innerHTML.includes(escaped),f.id+":"+s.id);
 assert.equal(get("state-title").textContent,s.name);
 assert(!get("screen").innerHTML.includes("undefined"));
}
app.selectFlow("ride");app.go("recording","newRide");
assert(get("screen").innerHTML.includes("00:00:00"));
app.go("paused");app.go("confirm-paused");app.go("paused");
assert.equal(app.getState().state,"paused");
app.go("saving");
assert(!get("screen").innerHTML.includes('class="d-btn'));
assert(!get("screen").innerHTML.includes('class="d-back'));
app.go("error");app.go("error-return");
assert(get("screen").innerHTML.includes("SAVE FAILED"));
app.go("free-map:preview");
app.go("ride:ready",undefined,true);
assert.equal(app.getState().state,"error"); // Back/map never discards failed file.
app.go("routes-app:verifying");
assert.equal(app.getState().flow,"routes-app");
assert(get("screen").innerHTML.includes("NOT SAVED YET"));
app.showView("system");assert(!get("system-view").hidden && get("flows-view").hidden);
app.showView("audit");assert(!get("audit-view").hidden);
app.selectFlow("free-map");
const click=dataset=>listener({target:{closest:()=>({dataset})}});
for(let i=0;i<10;i++)click({zoom:"1"});
assert.equal(app.getState().zoom,17);
for(let i=0;i<10;i++)click({zoom:"-1"});
assert.equal(app.getState().zoom,13);
const html=await readFile("index.html","utf8");
for(const id of elements.keys())if(id!=="caption")assert(html.includes('id="'+id+'"'),id);
for(const f of ["review.js","flows.js","more-flows.js","audit.js"]){
 const text=await readFile(f,"utf8");
 assert(!/fetch\s*\(|XMLHttpRequest|WebSocket|navigator\.bluetooth/.test(text));
}
console.log("PASS: "+flows.length+" flows, "+states+" states, "+edges+" linked actions/events; "+audit.length+" findings; all renders and guards checked.");
vm.runInNewContext(await readFile('compare.js','utf8'),ctx);
const compare=ctx.window.compareApp;
for(const [id,s] of Object.entries(compare.states)){
 compare.go(id);
 for(const [variant] of compare.variants){
  const rendered=compare.screen(variant);
  assert(!rendered.includes('undefined'),id+variant);
  assert.equal((rendered.match(/data-action=/g)||[]).length,s.actions.length);
  for(const a of s.actions){
   assert(rendered.includes('aria-label="'+a.long+'"'));
   assert(compare.states[a.next]||['zoomIn','zoomOut','center','command'].includes(a.next));
  }
 }
}
compare.select('recording');compare.go('paused');compare.go('confirm');compare.go('saved');
assert.equal(compare.getState().current,'saved');
compare.select('map');for(let i=0;i<10;i++)compare.go('zoomIn');assert.equal(compare.getState().zoom,17);
for(let i=0;i<10;i++)compare.go('zoomOut');assert.equal(compare.getState().zoom,13);
compare.select('camera');compare.go('command','Shutter');assert.equal(compare.getState().current,'camera');
assert(get('compare-feedback').textContent.includes('Command requested'));
assert(!/fetch\s*\(|XMLHttpRequest|WebSocket|navigator\.bluetooth/.test(await readFile('compare.js','utf8')));
console.log('PASS: comparison variants, action parity, accessible labels, transitions and zoom bounds.');
