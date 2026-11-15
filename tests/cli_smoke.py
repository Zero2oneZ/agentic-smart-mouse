#!/usr/bin/env python3
"""Exercise headless session commands in an isolated repository and database."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

app = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="hotbox-cli-") as temp:
    root = Path(temp)
    runtime=root / "runtime"
    runtime.mkdir(mode=0o700)
    os.environ["XDG_RUNTIME_DIR"]=str(runtime)
    source = root / "source"
    source.mkdir()
    data = root / "data"

    def git(*args):
        subprocess.run(["git", "-C", str(source), *args], check=True, capture_output=True)

    def call(command, *args, expected=0):
        result = subprocess.run(
            [app, command, "--data-dir", str(data), *args], text=True, capture_output=True
        )
        assert result.returncode == expected, (command, result.returncode, result.stdout, result.stderr)
        if not result.stdout and expected != 0:
            assert result.stderr
            return {}
        return json.loads(result.stdout)

    git("init")
    git("config", "user.name", "Hotbox Test")
    git("config", "user.email", "test@example.invalid")
    (source / "app.txt").write_text("initial\n")
    git("add", "app.txt")
    git("commit", "-m", "Initial")
    project = call(
        "focus", "--name", "CLI project", "--brief", "Test session workflow", "--folder", str(source)
    )["category"]
    category = str(project)
    event = call("run", "--category", category, "--", "/usr/bin/printf", "build passed\n")["event"]
    detail = call("read-event", "--category", category, "--id", str(event))
    assert "build passed" in detail["content"]
    call("snapshot", "--category", category)
    call("snapshot", "--category", category)
    call("branch", "--category", category)
    call("git-config", "--category", category, "--paths", "app.txt", "--automatic")
    call("sync", "--category", category)
    session = root / "session.jsonl"
    session.write_text('{"action":"review","result":"passed"}\n')
    call("attach", "--category", category, "--path", str(session))
    assert len(call("events", "--category", "1")["events"]) >= 7
    call("git-config", "--category", "999", "--paths", "app.txt", expected=2)
    segment = call("segment", "create", "--category", category, "--kind", "web_search", "--title", "Rendering research", "--description", "Compare rendering options")
    segment_id = segment["id"]
    original = call("segment", "add", "--category", category, "--id", segment_id, "--kind", "output", "--content", "Qt fits the native desktop.")
    call("segment", "add", "--category", category, "--id", segment_id, "--kind", "history", "--event", str(event))
    call("segment", "ready", "--category", category, "--id", segment_id)
    job = call("segment", "claim", "--category", category, "--worker", "cli-smoke")
    assert job["id"] == segment_id and job["lease"]
    finished = call("segment", "finish", "--category", category, "--id", segment_id, "--lease", job["lease"], "--description", "Rendering comparison", "--summary", "Prefer native Qt.", "--content", '{"choice":"Qt"}')
    assert finished["state"] == "complete"
    assert call("segment", "part", "--category", category, "--id", original["id"])["content"] == "Qt fits the native desktop."
    bundle = call("segment", "compose", "--category", category, "--title", "Implementation context", "--ids", segment_id)
    call("segment", "ready", "--category", category, "--id", bundle["id"])
    assert call("segment", "process", "--category", category)["state"] == "complete"
    assert len(call("segment", "list", "--category", category, "--state", "complete")["segments"]) == 2
    architecture = call("architecture", "init")
    assert architecture["ok"] and architecture["containerStatus"] == "not_configured"
    call("architecture", "define", "--id", "documents", "--relative-path", "libraries/documents", "--description", "Shared documents")
    library_workspace = str(call("focus", "--category", "1", "--name", "Storage project", "--folder", "")["category"])
    storage_args = ("--category", library_workspace, "--capabilities", "music,album-covers,documents", "--artist", "Sample Band", "--album", "Sample Album", "--genre", "Rock")
    preview = call("architecture", "plan", *storage_args)
    assert preview["ok"] and not Path(preview["source"]).exists()
    provisioned = call("architecture", "ensure", *storage_args)
    assert provisioned["ok"] and Path(provisioned["source"]).is_dir()
    assert call("architecture", "ensure", *storage_args)["ok"]
    registry = call("architecture", "catalog", "--category", library_workspace)
    assert registry["attachments"] and len(registry["genres"]) == 1
    call("architecture", "ensure", "--category", library_workspace, "--capabilities", "music", "--artist", "../escape", expected=1)
    build_request=root/'build-request.json'
    def build(operation,payload,expected=0):
        build_request.write_text(json.dumps({'schema_version':1,**payload}))
        return call('build',operation,'--category',category,'--request-file',str(build_request),expected=expected)
    assert build('schema',{})['api']=='hotbox.build'
    created=build('create',{'request_id':'road-create','title':'CLI roadmap','branch_ref':'declared-main'})
    roadmap=created['roadmap']
    assert build('create',{'request_id':'road-create','title':'CLI roadmap','branch_ref':'declared-main'})['replayed']
    step=build('node.add',{'request_id':'road-step','roadmap':roadmap,'expected_revision':1,'kind':'step','title':'Run a command'})
    command=call('run','--category',category,'--build',roadmap,'--step',step['node'],'--',sys.executable,'-c',"print('bound command passed')")
    assert command['buildLinked'] and command['terminalSession'].startswith('terminal:')
    state=build('read',{'roadmap':roadmap})
    assert len(state['links'])==2 and state['master_chat']==1
    document=build('document.save',{'request_id':'road-doc','roadmap':roadmap,'expected_revision':state['revision'],'node':step['node'],'title':'Build notes','content':'Initial version'})
    snapshot=build('checkpoint',{'request_id':'road-checkpoint','roadmap':roadmap,'expected_revision':document['revision']})
    second=build('document.save',{'request_id':'road-doc-two','roadmap':roadmap,'expected_revision':snapshot['revision'],'document':document['document'],'parent_version':document['version'],'content':'Later version'})
    branch=build('fork',{'request_id':'road-fork','roadmap':roadmap,'expected_revision':second['revision'],'checkpoint':snapshot['checkpoint'],'title':'Alternative build'})
    forked=build('read',{'roadmap':branch['roadmap']})
    first_version=forked['documents'][0]
    assert build('document.read',{'roadmap':branch['roadmap'],'version':first_version['version']})['content']=='Initial version'
    assert len(forked['links'])==2 and first_version['parent_version']==document['version']
    build('node.update',{'request_id':'fake-verify','roadmap':roadmap,'expected_revision':second['revision'],'node':step['node'],'state':'verified'},expected=1)
    failed=call('run','--category',category,'--build',roadmap,'--step',step['node'],'--',sys.executable,'-c','import sys; sys.exit(3)',expected=3)
    assert failed['buildIssueRecorded']
    assert any(node['id']==failed['buildIssue'] and node['kind']=='issue' for node in build('read',{'roadmap':roadmap})['nodes'])
    loop=call("optimize")
    assert loop["mode"]=="advisory_only" and loop["runs"]==0
    assert loop["policy"]["id"]=="master-loop-v1"
    call("optimize","--category",library_workspace,expected=1)
    health=call("doctor")
    assert health["ok"] and health["databaseOk"] and health["coreAvailable"]
    locations={row["name"]:row for row in health["storage"]}
    assert len({row["path"] for row in locations.values()}) == len(locations)
    tool_root=Path(architecture["toolRoot"])
    tracked=subprocess.run(["git","-C",str(tool_root),"ls-files"],capture_output=True,text=True,check=True).stdout
    assert not any(name in tracked for name in ("hotbox.sqlite3","orchestration.sqlite3","agent-runs","run.log"))
    assert data.joinpath("hotbox.sqlite3").exists()

print("CLI workflow passed: sessions, Git, segments, architecture registration, read-only plans, and reusable storage, versioned build documents, checkpoint forks and terminal bindings.")
