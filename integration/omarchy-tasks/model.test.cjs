const {test}=require('node:test');const assert=require('node:assert/strict');const model=require('./TaskModel.js');
test('bounded task data stays plain text, with known states and IDs',()=>{
 const id='a'.repeat(32),title='<img src="https://example.invalid/tracker">';
 assert.equal(model.parse(JSON.stringify({version:1,jobs:[{id,title,state:'running'}]}))[0].title,title);
 assert.deepEqual(model.parse(JSON.stringify({version:1,jobs:[{id:'bad; command',title,state:'running'},{id,title,state:'unknown'},{id,title,state:'finished',ack:true}]})),[]);
 assert.deepEqual(model.parse('x'.repeat(64001)),[]);
});
test('actions use argv and validated fixed views',()=>{
 const id='b'.repeat(32);assert.deepEqual(model.command('/home/example',id,'prompt'),['/home/example/.local/bin/hotbox','agent-ui',id,'prompt']);
 assert.equal(model.command('/home/example',id+';echo hi','chat'),null);assert.equal(model.command('/home/example',id,'arbitrary-command'),null);
});
