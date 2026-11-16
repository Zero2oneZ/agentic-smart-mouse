#!/usr/bin/env python3
"""Check the tool menu source schema, generated reference and compiled catalog."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--write', action='store_true')
parser.add_argument('--binary', type=Path)
args = parser.parse_args()
source = root / 'templates/tool-menu-api-v1.json'
raw = source.read_bytes()
catalog = json.loads(raw)
assert catalog['api'] == 'hotbox.tool-menu' and catalog['schema_version'] == 1
rust = (root / 'src/toolmenu.rs').read_text()
for operation, struct in [('tool_menu_register', 'ToolMenuRegistration'), ('tool_menu_update', 'ToolMenuUpdate')]:
    body = re.search(r'pub struct ' + struct + r' \{(.*?)\n\}', rust, re.S).group(1)
    fields = re.findall(r'pub (\w+):', body)
    assert fields == catalog['operations'][operation]['request'], (operation, fields)
main = (root / 'src/main.rs').read_text()
for operation, variant in [('tool_menu_read', 'ToolMenuRead'), ('tool_menu_list', 'ToolMenuList')]:
    body = re.search(r'\b' + variant + r' \{(.*?)\}', main, re.S).group(1)
    assert re.findall(r'(\w+):', body) == catalog['operations'][operation]['fields'], operation
assert catalog['actions'] == [
    {'id': 'edit', 'available': True},
    {'id': 'run', 'available': False, 'reason': 'No verified runtime release or execution adapter is registered.'},
]
lines = ['# Tool menu API reference', '',
         'Generated from [tool-menu-api-v1.json](../templates/tool-menu-api-v1.json).', '',
         'Source SHA-256: `' + hashlib.sha256(raw).hexdigest() + '`.', '',
         'The core owns definition identity and metadata. Qt owns the mapped private',
         'development chat and replayable projection. These operations do not execute',
         'tools, create verified releases, copy source or grant host permissions.', '',
         '| Command | Fields |', '| --- | --- |']
for name, operation in catalog['operations'].items():
    nested = 'request' in operation
    fields = ', '.join('`' + field + '`' for field in operation.get('request', operation.get('fields')))
    lines.append('| `' + name + '` | ' + ('Nested `request`: ' if nested else '') + fields + ' |')
lines += ['', '`initial` is null for an unversioned draft, or an immutable `ToolVersion`.',
          'Registering an existing tool requires its exact version and originating chat.',
          'Mutations require schema version 1 and a unique request ID. Exact replays return',
          'the original receipt without overwriting newer metadata. Changed input with the',
          'same ID conflicts. Updates require `expected_revision`; only display name,',
          'purpose and menu category change. Context and source identity remain fixed.', '',
          'Reads and mutations are resource-owner-only. Lists use `after` as an exclusive',
          'tool-ID cursor and return at most 100 entries. An empty list ends pagination.',
          'The trusted local host supplies actor identity; this is not authentication.', '',
          '`tool-menu-catalog` returns this catalog without opening a database.',
          'The action set is Edit plus unavailable Run. No runtime is launched by',
          'registration, menu discovery, navigation, or metadata updates.', '',
          'Core schema 4 adds menu metadata and idempotent request receipts. Existing',
          'tool/version records and workflow state remain in their original tables.', '',
          'Implementation: [toolmenu.rs](../src/toolmenu.rs), [lineage.rs](../src/lineage.rs),',
          '[CLI](../src/main.rs). Checks: [library tests](../tests/toolmenu.rs),',
          '[CLI checks](../tests/test_tool_menu_cli.py).', '']
reference = root / 'docs/TOOL_MENU_REFERENCE.md'
rendered = '\n'.join(lines)
if args.write:
    reference.write_text(rendered)
assert reference.read_text() == rendered, 'Tool menu reference is stale; run core/scripts/check_tool_menu.py --write'
if args.binary:
    envelope = json.loads(subprocess.check_output([str(args.binary.resolve()), 'tool-menu-catalog'], text=True))
    assert envelope['ok'] and envelope['result'] == catalog, 'Compiled tool menu catalog differs from source'
print('Tool menu contract passed: request fields, reference' + (', compiled catalog.' if args.binary else '.'))
