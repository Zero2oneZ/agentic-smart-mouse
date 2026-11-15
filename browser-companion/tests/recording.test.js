import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {safeUrl,sanitizeStep,exported} from '../sanitize.js';
test('URLs omit query, fragment, credentials and sensitive paths',()=>{
 assert.equal(safeUrl('https://example.org/search?q=private&token=secret#auth'),'https://example.org/search');
 assert.equal(safeUrl('https://name:password@example.org/'),null);
 assert.equal(safeUrl('file:///home/user/Cookies'),null);
 assert.equal(safeUrl('https://example.org/profile/alice'),'https://example.org/');
});
test('export is whitelist-only and contains no input values or auth fields',()=>{
 const step={kind:'input_field',url:'https://example.org/search?q=secret',target:'Search',value:'private-value',cookies:'session=private',authorization:'Bearer secret'};
 const out=JSON.stringify(exported({steps:[step,{...step,target:'Password'},{kind:'execute',url:'https://example.org',script:'danger'}]}));
 assert.equal(JSON.parse(out).steps.length,1);for(const forbidden of ['private-value','cookies','authorization','Bearer','danger','q=secret'])assert.ok(!out.includes(forbidden));
 assert.equal(sanitizeStep({...step,kind:'shortcut',target:'Ctrl+V'}),null);
});
test('content capture never reads values and ignores passwords and synthetic events',async()=>{
 const handlers={};const sent=[];
 const context={location:{origin:'https://example.org',pathname:'/form'},document:{addEventListener:(type,fn)=>handlers[type]=fn},chrome:{runtime:{sendMessage:message=>{sent.push(message);return Promise.resolve();},onMessage:{addListener:()=>{}}}}};
 vm.runInNewContext(fs.readFileSync(new URL('../content.js',import.meta.url),'utf8'),context);
 const element={tagName:'INPUT',getAttribute:key=>key==='name'?'Search':null};Object.defineProperty(element,'value',{get:()=>{throw new Error('input values must never be read');}});
 handlers.change({isTrusted:true,target:element});assert.equal(sent.length,1);
 handlers.change({isTrusted:false,target:element});assert.equal(sent.length,1);
 handlers.change({isTrusted:true,target:{tagName:'INPUT',getAttribute:key=>key==='type'?'password':null}});assert.equal(sent.length,1);
});
test('manifest has no cookies, history, broad host access or content scripts',()=>{
 const manifest=JSON.parse(fs.readFileSync(new URL('../manifest.json',import.meta.url)));
 assert.deepEqual(manifest.permissions,['activeTab','scripting','storage']);assert.equal(manifest.host_permissions,undefined);assert.equal(manifest.content_scripts,undefined);
});
