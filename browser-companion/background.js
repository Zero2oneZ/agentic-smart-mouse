import {safeUrl, sanitizeStep, exported} from './sanitize.js';
let chain = Promise.resolve();
function serial(operation) { const next = chain.then(operation,operation); chain = next.catch(()=>{}); return next; }
async function read() { return (await chrome.storage.local.get('recording')).recording || {active:false,steps:[]}; }
async function save(state) {
  await chrome.storage.local.set({recording:state});
  await chrome.action.setBadgeText({text:state.active?'REC':''});
  await chrome.action.setBadgeBackgroundColor({color:'#a53636'});
}
async function pause(state, reason='') {
  state.active=false;state.problem=reason;
  if(state.tabId !== undefined) await chrome.tabs.sendMessage(state.tabId,{type:'hotbox-stop'}).catch(()=>{});
  await save(state);return state;
}
async function attach(state) { await chrome.scripting.executeScript({target:{tabId:state.tabId},files:['content.js']}); }
async function appendStep(state,step) {
 const previous=state.steps.at(-1);
 if(previous && JSON.stringify(previous)===JSON.stringify(step))return {ok:true};
 if(state.steps.length>=500 || new TextEncoder().encode(JSON.stringify([...state.steps,step])).length>230000){await pause(state,'Recording is full. Export it before starting another.');return {ok:false};}
 state.steps.push(step);await save(state);return {ok:true};
}
async function handle(message,sender) {
  let state=await read();
  if(message.type==='step') {
    if(!state.active || sender.tab?.id!==state.tabId || sender.frameId!==0) return {ok:false};
    const step=sanitizeStep(message.step);
    const senderUrl=safeUrl(sender.url || '');
    if(!step || !senderUrl || new URL(step.url).origin!==state.origin || new URL(senderUrl).origin!==state.origin) return {ok:false};
    return appendStep(state,step);
  }
  if(sender.tab) return {ok:false}; // Only the extension's own popup may control or export a session.
  if(message.type==='state') return {ok:true,state};
  if(message.type==='stop') return {ok:true,state:await pause(state)};
  if(message.type==='export') {await pause(state);return {ok:true,recording:exported(state)};}
  if(message.type==='start') {
    if(state.active) return {ok:false,error:'Finish this recording first.'};
    const [tab]=await chrome.tabs.query({active:true,currentWindow:true});
    const url=safeUrl(tab?.url);
    if(!url) return {ok:false,error:'Choose a normal web page first. Browser settings pages are not recording targets.'};
    state={active:true,tabId:tab.id,origin:new URL(url).origin,steps:[{kind:'navigate',url,target:''}],problem:''};
    try {await attach(state);await save(state);return {ok:true,state};}
    catch {return {ok:true,state:await pause(state,'This page does not allow recording. Try a regular web page.')};}
  }
  return {ok:false,error:'Unsupported recording action.'};
}
chrome.runtime.onMessage.addListener((message,sender,respond)=>{serial(()=>handle(message,sender)).then(respond).catch(()=>respond({ok:false,error:'Could not update the recording.'}));return true;});
chrome.tabs.onUpdated.addListener((tabId,change,tab)=>{if(change.status==='complete')serial(async()=>{
  const state=await read();if(!state.active||state.tabId!==tabId)return;
  const url=safeUrl(tab.url);if(!url||new URL(url).origin!==state.origin){await pause(state,'Changing sites paused recording. Export, then start explicitly on the new site.');return;}
  const saved=await appendStep(state,{kind:'navigate',url,target:''});
  if(!saved.ok)return;
  try {await attach(state);}catch{await pause(state,'Recording paused because this page is unavailable.');}
});});
chrome.tabs.onRemoved.addListener(tabId=>serial(async()=>{const state=await read();if(state.active&&state.tabId===tabId)await pause(state,'The recorded tab was closed.');}));
chrome.runtime.onStartup.addListener(()=>serial(async()=>pause(await read(),'Recording paused after browser restart.')));
chrome.runtime.onInstalled.addListener(()=>serial(async()=>pause(await read())));

chrome.commands.onCommand.addListener(command=>{if(command==='toggle-recording')serial(async()=>{
 const state=await read();
 if(state.active)await pause(state);
 else if(state.steps.length){state.problem='Export this recording before starting another from the popup.';await save(state);}
 else await handle({type:'start'},{});
});});
