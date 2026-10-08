#!/usr/bin/env node
// Build-time compression only: no minifier code is shipped to site visitors.
'use strict';
const fs = require('node:fs/promises');
const path = require('node:path');
const args = process.argv.slice(2);
if (args.length !== 2) {
  console.error('Usage: node scripts/optimize_website.cjs <website-directory> <tool-node_modules>');
  process.exit(2);
}
const root = path.resolve(args[0]);
const tools = path.resolve(args[1]);
const { minify: minifyHTML } = require(path.join(tools, 'html-minifier-terser'));
const { minify: minifyJS } = require(path.join(tools, 'terser'));
const CleanCSS = require(path.join(tools, 'clean-css'));
async function* files(directory) {
  for (const entry of await fs.readdir(directory, {withFileTypes:true})) {
    if (['.git','api','node_modules'].includes(entry.name)) continue;
    const file = path.join(directory,entry.name);
    if (entry.isDirectory()) yield* files(file);
    else yield file;
  }
}
(async () => {
  let before=0,after=0,count=0;
  for await (const file of files(root)) {
    const extension=path.extname(file);
    if (!['.html','.css','.js'].includes(extension)) continue;
    const source=await fs.readFile(file,'utf8');
    let output;
    if (extension==='.html') {
      output=await minifyHTML(source,{collapseWhitespace:true,removeComments:true,removeRedundantAttributes:true,minifyCSS:true,minifyJS:true,keepClosingSlash:true});
    } else if (extension==='.css') {
      const result=new CleanCSS({level:2,rebase:false}).minify(source);
      if (result.errors.length) throw new Error(result.errors.join('\n'));
      output=result.styles;
    } else {
      const result=await minifyJS(source,{compress:true,mangle:true,format:{comments:false}});
      if (!result.code) throw new Error(`No JavaScript output for ${file}`);
      output=result.code;
    }
    await fs.writeFile(file,output+'\n');
    before+=Buffer.byteLength(source);after+=Buffer.byteLength(output+'\n');count++;
  }
  console.log(`Compressed ${count} HTML/CSS/JS files: ${before} → ${after} bytes (${Math.round(100*(1-after/before))}% smaller)`);
})().catch(error=>{console.error(error);process.exitCode=1;});
