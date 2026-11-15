#!/usr/bin/env python3
"""Exercise headless session commands in an isolated repository and database."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

app = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="hotbox-cli-") as temp:
    root = Path(temp)
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
print("CLI workflow passed: events, commands, Git, segment intake, claim/finish, original evidence, and bundle composition.")
