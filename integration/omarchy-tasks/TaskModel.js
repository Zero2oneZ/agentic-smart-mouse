function parse(text) {
  if(typeof text!=="string" || text.length>64000)return []
  try {
    var data=JSON.parse(text)
    if(data.version!==1 || !Array.isArray(data.jobs))return []
    return data.jobs.slice(0,50).filter(function(job){return job && /^[a-f0-9]{32}$/.test(job.id) && ["running","finished","attention"].indexOf(job.state)>=0 && typeof job.title==="string" && !job.ack}).map(function(job){return {id:job.id,title:job.title.slice(0,80),state:job.state,provider:String(job.provider||"").slice(0,12),created:String(job.created||"").slice(0,30)}})
  } catch(e){return []}
}
function command(home,id,view) {
  if(!/^[a-f0-9]{32}$/.test(id) || ["chat","terminal","prompt"].indexOf(view)<0)return null
  return [home+"/.local/bin/hotbox","agent-ui",id,view]
}
if(typeof module!=="undefined")module.exports={parse:parse,command:command}
