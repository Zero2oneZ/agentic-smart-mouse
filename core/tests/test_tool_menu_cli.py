#!/usr/bin/env python3
"""Real-process tool menu protocol tests using temporary state and fixture metadata."""
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='hotbox-tool-menu-cli-') as temporary:
    database = Path(temporary) / 'core.sqlite3'
    def call(command, ok=True):
        result = subprocess.run([str(binary), str(database)], input=json.dumps(command), capture_output=True, text=True)
        value = json.loads(result.stdout)
        assert value['ok'] == ok and (result.returncode == 0) == ok, value
        return value.get('result', value)
    request = dict(schema_version=1, request_id='fixture-register', actor='alice', tool_id='formatter', resource='project',
                   name='Brief formatter', purpose='Format fixture notes', menu_category='Writing', originating_chat='qt-fixture', initial=None)
    invalid = dict(request, unexpected='not allowed')
    call(dict(command='tool_menu_register', request=invalid), False)
    assert not database.exists(), 'Unknown request fields must fail before opening a database'
    call(dict(command='resource', actor='alice', resource='project'))
    first = call(dict(command='tool_menu_register', request=request))
    assert call(dict(command='tool_menu_register', request=request)) == first
    assert first['selected'] is None
    renamed = dict(schema_version=1, request_id='fixture-rename', actor='alice', tool_id='formatter', expected_revision=1,
                   name='Brief writer', purpose='Format fixture notes', menu_category='Editing')
    second = call(dict(command='tool_menu_update', request=renamed))
    assert second['revision'] == 2 and second['originating_chat'] == first['originating_chat']
    assert call(dict(command='tool_menu_register', request=request)) == first
    assert call(dict(command='tool_menu_read', actor='alice', tool_id='formatter')) == second
    assert call(dict(command='tool_menu_list', actor='alice', resource='project', after='')) == [second]
    call(dict(command='tool_menu_read', actor='bob', tool_id='formatter'), False)
    call(dict(command='tool_menu_update', request=dict(renamed, request_id='stale')), False)
    with sqlite3.connect(database) as db:
        assert db.execute('select count(*) from tools').fetchone()[0] == 1
        assert db.execute('select count(*) from runs').fetchone()[0] == 0
print('Tool menu CLI passed: strict input, replay, rename, current context, owner checks, and no runtime launch.')
