#!/usr/bin/env python3
"""Build the CLI image from explicit pinned bases; never push or launch an image."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def pinned(value):
    if not re.fullmatch(r"[a-z0-9][a-z0-9._/:-]*@sha256:[a-f0-9]{64}", value):
        raise argparse.ArgumentTypeError("image must be an explicit repository@sha256:<64 lowercase hex> reference")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--builder-image", required=True, type=pinned,
                        help="reviewed Rust >=1.89 bookworm image, pinned by digest")
    parser.add_argument("--runtime-image", required=True, type=pinned,
                        help="reviewed Debian bookworm-slim image, pinned by digest")
    parser.add_argument("--engine", choices=["docker", "podman"], default="docker")
    parser.add_argument("--tag", default="hotbox-core:local")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-z0-9][a-z0-9._/-]*(?::[A-Za-z0-9_.-]+)?", args.tag):
        parser.error("tag must be a local image name with an optional tag")
    root = Path(__file__).resolve().parent.parent
    command = [args.engine, "build", "--file", str(root / "Containerfile"),
               "--build-arg", "BUILDER_IMAGE=" + args.builder_image,
               "--build-arg", "RUNTIME_IMAGE=" + args.runtime_image,
               "--tag", args.tag, str(root)]
    if args.dry_run:
        print(json.dumps({"command": command, "push": False, "launch": False}))
    else:
        subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
