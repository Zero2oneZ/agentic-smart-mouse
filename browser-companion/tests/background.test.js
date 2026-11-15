import test from 'node:test';
import assert from 'node:assert/strict';
test('selected-tab boundary, private export, same-origin navigation and restart pause',async()=>{
 let recording;const handlers={};let injections=0;
 globalThis.chrome={
  storage:{local:{get:async()=>({recording:structuredClone(recording)}),set:async value=>{recording=structuredClone(value.recording);}}},
  action:{setBadgeText:async()=>{},setBadgeBackgroundColor:async()=>{}},
  scripting:{executeScript:async()=>{injections++;}},
  tabs:{query:async()=>[{id:7,url:'https://example.org/start?q=private'}],sendMessage:async()=>{},onUpdated:{addListener:fn=>handlers.updated=fn},onRemoved:{addListener:fn=>handlers.removed=fn}},
  runtime:{onMessage:{addListener:fn=>handlers.message=fn},onStartup:{addListener:fn=>handlers.startup=fn},onInstalled:{addListener:fn=>handlers.installed=fn}},
  commands:{onCommand:{addListener:fn=>handlers.command=fn}}
 };
 await import('../background.js');
 const send=(message,sender={})=>new Promise(resolve=>handlers.message(message,sender,resolve));
 assert.equal((await send({type:'start'})).ok,true);assert.equal(recording.active,true);assert.equal(injections,1);
 const event={type:'step',step:{kind:'input_field',url:'https://example.org/start?token=secret',target:'Search',value:'never-save-value',cookies:'private-cookie'}};
 assert.equal((await send(event,{tab:{id:8},frameId:0,url:'https://example.org/start'})).ok,false);
 assert.equal((await send(event,{tab:{id:7},frameId:1,url:'https://example.org/start'})).ok,false);
 assert.equal((await send(event,{tab:{id:7},frameId:0,url:'https://example.org/start'})).ok,true);
 handlers.updated(7,{status:'complete'},{url:'https://example.org/next?private=yes'});
 await send({type:'state'});assert.equal(recording.steps.at(-1).url,'https://example.org/next');
 handlers.updated(7,{status:'complete'},{url:'https://another.example.org/'});
 await send({type:'state'});assert.equal(recording.active,false);
 const result=await send({type:'export'});const text=JSON.stringify(result.recording);for(const word of ['never-save-value','private-cookie','token=secret','q=private'])assert.ok(!text.includes(word));
 await handlers.startup();assert.equal(recording.active,false);
});
