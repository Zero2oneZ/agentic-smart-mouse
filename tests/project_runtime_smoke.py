#!/usr/bin/env python3
"""Check project cloud routing and local installation with temporary mock tools."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='hotbox-project-runtime-') as temporary:
    base = Path(temporary)
    project = base / 'project'
    (project / 'bin').mkdir(parents=True)
    (project / 'core/templates').mkdir(parents=True)
    for name in ('launch-role', 'project-shell', 'project_runtime.py', 'install-ollama'):
        shutil.copy2(ROOT / 'bin' / name, project / 'bin' / name)
    shutil.copy2(ROOT / 'core/templates/ollama-role-models-v1.json', project / 'core/templates/ollama-role-models-v1.json')
    data = base / 'private-data'
    data.mkdir(mode=0o700)
    secret = data / 'secrets.env'
    secret.write_text('export OLLAMA_API_KEY="sample-terminal-key"\n')
    secret.chmod(0o600)
    tools = base / 'tools'
    tools.mkdir()
    claude = tools / 'claude'
    claude.write_text('#!' + sys.executable + '\n' + '''import json, os, sys
assert os.environ['ANTHROPIC_AUTH_TOKEN'] == 'sample-terminal-key'
assert os.environ['ANTHROPIC_API_KEY'] == ''
assert os.environ['ANTHROPIC_BASE_URL'] == 'https://ollama.com'
print(json.dumps({'args':sys.argv[1:], 'model':os.environ['ANTHROPIC_DEFAULT_OPUS_MODEL'], 'cwd':os.getcwd()}))
''')
    claude.chmod(0o755)
    env = dict(os.environ, PATH=str(tools) + ':/usr/bin', HOTBOX_DATA_DIR=str(data))
    env.pop('OLLAMA_API_KEY', None)
    def run(name, *args, check=True):
        return subprocess.run([str(project / 'bin' / name), *args], cwd=base, env=env, text=True, capture_output=True, check=check)
    for role, model in [('builder', 'deepseek-v4.1-flash'), ('vision', 'inkling-2-small')]:
        result = run('launch-role', role, '--', '-p', 'Sample task')
        assert 'sample-terminal-key' not in result.stdout + result.stderr
        observed = json.loads(result.stdout)
        assert observed == {'args':['--model', model, '-p', 'Sample task'], 'model':model, 'cwd':str(base)}
    secret.chmod(0o644)
    assert run('launch-role', 'builder', check=False).returncode != 0
    secret.chmod(0o600)
    request = json.loads(run('launch-role', 'vision', '--supervised', '--category', '7', '--prompt', 'Look', '--dry-run').stdout)
    assert request == {'operation':'cloud-role.start','role':'vision','category':7,'prompt':'Look'}
    assert run('launch-role', 'builder', '--supervised', '--prompt', 'x'*4001, '--dry-run', check=False).returncode != 0
    assert run('launch-role', 'builder', '--via', 'ollama', check=False).returncode != 0
    # Install the native entry point using a local archive and a mock downloader.
    fixture = base / 'fixture'
    (fixture / 'bin').mkdir(parents=True)
    (fixture / 'bin/ollama').write_text('#!/bin/sh\nprintf "mock Ollama\\n"\n')
    archive = base / 'mock-ollama.tar.zst'
    subprocess.run(['tar','--zstd','--create','--file',str(archive),'--directory',str(fixture),'bin/ollama'],check=True)
    curl = tools / 'curl'
    curl.write_text('#!' + sys.executable + '\nimport os, shutil, sys\nshutil.copyfile(os.environ["MOCK_ARCHIVE"], sys.argv[sys.argv.index("--output")+1])\n')
    curl.chmod(0o755)
    env['MOCK_ARCHIVE'] = str(archive)
    run('install-ollama')
    installed = project / '.tools/ollama/bin/ollama'
    assert installed.is_file() and os.access(installed, os.X_OK)
    assert json.loads((project / '.tools/ollama/installation.json').read_text())['scope'] == 'cloud-cli-only'
    assert run('project-shell', 'ollama').stdout == 'mock Ollama\n'
    # Upgrade an existing CLI installation to the complete runner package.
    (fixture / 'lib/ollama').mkdir(parents=True)
    runner = fixture / 'lib/ollama/llama-server'
    runner.write_text('#!/bin/sh\nexit 0\n')
    runner.chmod(0o755)
    subprocess.run(['tar','--zstd','--create','--file',str(archive),'--directory',str(fixture),'bin/ollama','lib/ollama'],check=True)
    run('install-ollama', '--full')
    assert (project / '.tools/ollama/lib/ollama/llama-server').is_file()
    assert json.loads((project / '.tools/ollama/installation.json').read_text())['scope'] == 'full'
    assert 'already installed' in run('install-ollama', '--full').stdout
    assert run('launch-role', 'builder', '--via', 'ollama', '--dry-run').stdout.startswith(str(installed)+' launch claude --model deepseek-v4.1-flash:cloud')
print('Project runtime checks passed: both roles, private key, shell paths, bounded requests, and local mock installation.')
