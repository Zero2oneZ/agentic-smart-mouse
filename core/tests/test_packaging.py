"""Packaging admission checks. These tests never access a container daemon."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/package_image.py"
BUILDER = "docker.io/library/rust:1.89-bookworm@sha256:" + "a" * 64
RUNTIME = "docker.io/library/debian:bookworm-slim@sha256:" + "b" * 64


class PackagingAdmission(unittest.TestCase):
    def invoke(self, *extra, builder=BUILDER):
        return subprocess.run([sys.executable, str(SCRIPT), "--dry-run",
                               "--builder-image", builder, "--runtime-image", RUNTIME,
                               *extra], text=True, capture_output=True, timeout=10)

    def test_mutable_or_injected_base_references_are_rejected(self):
        for value in ["rust:latest", "rust@sha256:123", "$(id)@sha256:" + "a" * 64]:
            response = self.invoke(builder=value)
            self.assertNotEqual(response.returncode, 0)
            self.assertEqual(response.stdout, "")

    def test_packaging_is_build_only_with_narrow_source_context(self):
        response = self.invoke()
        self.assertEqual(response.returncode, 0, response.stderr)
        result = json.loads(response.stdout)
        self.assertFalse(result["push"])
        self.assertFalse(result["launch"])
        self.assertEqual(result["command"][1], "build")
        self.assertEqual(Path(result["command"][-1]), SCRIPT.parent.parent)

    def test_engine_and_tag_cannot_inject_actions(self):
        for options in [("--engine", "sh"), ("--tag", "--push")]:
            self.assertNotEqual(self.invoke(*options).returncode, 0)


if __name__ == "__main__":
    unittest.main()
