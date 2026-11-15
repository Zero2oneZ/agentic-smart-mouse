#!/usr/bin/env python3
"""Exercise the public JSON protocol in fresh processes and a temporary database."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

binary, template_path = sys.argv[1:]
template = json.loads(Path(template_path).read_text())
actions = sorted({a for s in template["segments"] for a in s["required_actions"]})

with tempfile.TemporaryDirectory(prefix="hotbox-core-cli-") as temporary:
    database = str(Path(temporary) / "workflow.sqlite3")

    def call(command, succeeds=True, **arguments):
        process = subprocess.run(
            [binary, database], input=json.dumps({"command": command, **arguments}),
            text=True, capture_output=True, timeout=15, check=False,
        )
        response = json.loads(process.stdout)
        assert response["ok"] is succeeds, response
        assert (process.returncode == 0) is succeeds, process.stderr
        return response.get("result", response)

    listed = subprocess.run([binary, "catalog"], text=True, capture_output=True, timeout=15, check=True)
    listing = json.loads(listed.stdout)["result"]
    catalog_file = Path(temporary) / "catalog.json"
    catalog_file.write_text(json.dumps(listing["catalog"]))
    checked = subprocess.run([binary, "validate-catalog", str(catalog_file)], text=True,
                             capture_output=True, timeout=15, check=True)
    assert json.loads(checked.stdout)["result"]["catalog_hash"] == listing["catalog_hash"]
    invalid = listing["catalog"]
    invalid["categories"][0]["belongings_path"] = "../../escape"
    catalog_file.write_text(json.dumps(invalid))
    rejected = subprocess.run([binary, "validate-catalog", str(catalog_file)], text=True,
                              capture_output=True, timeout=15, check=False)
    assert rejected.returncode != 0 and json.loads(rejected.stdout)["ok"] is False
    assert not Path(database).exists(), "Catalog commands must not create a database"

    validated = subprocess.run([binary, "validate", template_path], text=True, capture_output=True, check=True)
    assert json.loads(validated.stdout)["ok"]
    call("resource", actor="alice", resource="project")

    # Pin one shared expiry so later child processes cannot accidentally extend it.
    expiry = int(time.time()) + 600

    def grant(identifier, issuer, grantee, parent=None):
        return call("grant", request={
            "id": identifier, "issuer": issuer, "grantee": grantee, "resource": "project",
            "parent": parent, "actions": actions, "may_delegate": True, "expires_at": expiry,
        })

    grant("alice-root", "alice", "alice")
    request = {"id": "fixture", "actor": "alice", "resource": "project", "grant_id": "alice-root",
               "template": template, "inputs": {"goal": "Verify the orchestration protocol"}}
    started = call("start", request=request)
    assert started["status"] == "running"
    # Each loop starts a separate process: SQL is the checkpoint source.
    partial = call("fixture_loop", actor="alice", run_id="fixture", worker="fixture-worker", max_steps=3)
    assert partial["executed"] == 3 and partial["run"]["status"] == "running"
    completed = call("fixture_loop", actor="alice", run_id="fixture", worker="fixture-worker", max_steps=8)
    assert completed["executed"] == 5 and completed["run"]["status"] == "complete"
    assert completed["run"]["template_hash"] == started["template_hash"]
    again = call("start", request=request)
    assert again["operations"] == 8 and again["status"] == "complete"
    events = call("events", actor="alice", run_id="fixture", after=0)["events"]
    assert len([e for e in events if e["kind"] == "segment.verified"]) == 8
    cursor = events[-1]["id"]
    assert call("events", actor="alice", run_id="fixture", after=cursor)["events"] == []
    assert "token" not in json.dumps(events)

    grant("alice-bob", "alice", "bob")
    grant("bob-carol", "bob", "carol", "alice-bob")
    grant("direct-carol", "alice", "carol")
    shared = {**request, "id": "shared", "actor": "carol", "grant_id": "bob-carol"}
    independent = {**shared, "id": "independent", "grant_id": "direct-carol"}
    call("start", request=shared)
    call("start", request=independent)
    lease = call("claim", actor="carol", run_id="shared", worker="worker")["claim"]
    assert lease["segment"]["stage"] == "discover"
    call("revoke", actor="alice", grant_id="alice-bob")
    call("claim", succeeds=False, actor="carol", run_id="shared", worker="worker")
    call("status", succeeds=False, actor="carol", run_id="shared")
    assert call("cancel", actor="carol", run_id="shared")["status"] == "cancelled"
    assert call("status", actor="carol", run_id="independent")["status"] == "running"
    call("status", succeeds=False, actor="bob", run_id="independent")

    version = {"version": "1.0.0", "source_commit": "a" * 40,
               "architecture_version": "architecture-v1", "originating_chat": "chat-original"}
    call("register_tool", request={"actor": "alice", "tool_id": "player", "resource": "project", "initial": version})
    context = call("tool_context", actor="alice", grant_id="alice-root", tool_id="player", version="1.0.0")
    assert context["selected"] == version and context["forked_from"] is None
    call("tool_version", actor="alice", tool_id="player", version={**version, "source_commit": "b" * 40}, succeeds=False)
    call("tool_version", actor="alice", tool_id="player", version={**version, "version": "2.0.0"})
    call("resource", actor="alice", resource="fork-project")
    fork = call("fork_tool", request={"actor": "alice", "grant_id": "alice-root",
        "source": {"tool_id": "player", "version": "1.0.0"}, "tool_id": "player-fork",
        "resource": "fork-project", "originating_chat": "chat-fork"})
    assert fork["selected"]["originating_chat"] == "chat-fork"
    assert fork["forked_from"] == {"tool_id": "player", "version": "1.0.0"}
    call("tool_context", actor="alice", grant_id="alice-root", tool_id="player-fork", version="1.0.0", succeeds=False)

print("CLI fixture passed: segmented build, restart/resume, immutable template, audit events, delegated revocation, independent access, immutable tool versions, fork lineage and catalog validation.")
