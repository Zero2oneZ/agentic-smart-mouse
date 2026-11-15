let count=0;
const status=document.querySelector('#status');
async function call(type) {const result=await chrome.runtime.sendMessage({type});if(!result.ok)throw new Error(result.error||'Recording unavailable');return result;}
function show(state) {count=state.steps.length;status.textContent=(state.active?'Recording this tab':'Recording stopped')+` · ${count} steps`+(state.problem?'\n'+state.problem:'');document.querySelector('#start').disabled=state.active;document.querySelector('#stop').disabled=!state.active;document.querySelector('#export').disabled=!count;}
for(const type of ['start','stop','export'])document.querySelector('#'+type).addEventListener('click',async()=>{
 try {
  if(type==='start'&&count&&!confirm('Start a new recording? Export the current one first if you want to keep it.'))return;
  const result=await call(type);
  if(type==='export') {const link=document.querySelector('#download');if(link.dataset.url)URL.revokeObjectURL(link.dataset.url);link.href=URL.createObjectURL(new Blob([JSON.stringify(result.recording,null,2)],{type:'application/json'}));link.dataset.url=link.href;link.hidden=false;link.focus();}
  show((await call('state')).state);
 }catch(error){status.textContent=error.message;}
});
call('state').then(result=>show(result.state)).catch(error=>{status.textContent=error.message;});
