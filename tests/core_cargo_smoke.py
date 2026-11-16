#!/usr/bin/env python3
"""Verify Cargo subprocess routing without downloading or changing host toolchains."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

source = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='hotbox-core-cargo-') as temporary:
    root = Path(temporary) / 'project'
    (root / 'bin').mkdir(parents=True)
    shutil.copy2(source / 'bin/core-cargo', root / 'bin/core-cargo')
    tools = root / '.tools/rust/bin'
    tools.mkdir(parents=True)
    fake = tools / 'cargo'
    fake.write_text('#!' + sys.executable + '\n' + '''import json, os, sys
print(json.dumps({'args':sys.argv[1:],'cache':os.environ['CARGO_HOME'],
 'rustc':os.environ.get('RUSTC'),'rustdoc':os.environ.get('RUSTDOC'),
 'path':os.environ['PATH'].split(os.pathsep)[0]}))
''')
    fake.chmod(0o755)
    env = os.environ.copy()
    for key in ('CARGO_HOME', 'RUSTC', 'RUSTDOC'):
        env.pop(key, None)
    run = lambda: json.loads(subprocess.check_output(
        [sys.executable, str(root / 'bin/core-cargo'), 'fetch', '--locked'], env=env, text=True))
    result = run()
    assert result['args'] == ['fetch', '--locked']
    assert result['cache'] == str(root / '.tools/cargo')
    assert result['rustc'] == str(tools / 'rustc')
    assert result['rustdoc'] == str(tools / 'rustdoc')
    assert result['path'] == str(tools)
    env.update(CARGO_HOME=str(root / 'custom-cache'), RUSTC='/custom/rustc', RUSTDOC='/custom/rustdoc')
    result = run()
    assert result['cache'] == str(root / 'custom-cache')
    assert result['rustc'] == '/custom/rustc' and result['rustdoc'] == '/custom/rustdoc'
    fake.unlink()
    env['PATH'] = str(root / 'no-tools')
    result = subprocess.run([sys.executable, str(root / 'bin/core-cargo'), '--version'], env=env, capture_output=True, text=True)
    assert result.returncode != 0 and 'Rust/Cargo is unavailable' in result.stderr
print('Core Cargo routing passed: local compiler/cache, explicit overrides, and missing-tool failure.')
