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
print("CLI workflow passed: focus, command capture, event read, snapshots, branch, settings, checkpoint, attachment, master timeline.")
