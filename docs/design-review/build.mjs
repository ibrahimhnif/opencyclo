import {mkdir,copyFile,cp,readFile} from "node:fs/promises";
import vm from "node:vm";
const ctx={window:{}};vm.runInNewContext(await readFile("flows.js","utf8"),ctx);
if(!ctx.window.REVIEW.flows.length)throw Error("No flows");
await mkdir("dist",{recursive:true});
for(const f of ["index.html","review.css","review.js","flows.js","more-flows.js","audit.js","compare.html","compare.css","compare.js"])await copyFile(f,"dist/"+f);
await cp("assets","dist/assets",{recursive:true});
console.log("Built static design review.");
