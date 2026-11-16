#!/usr/bin/env python3
"""Ensure stale references, broken source links and compiled drift fail closed."""
from contextlib import redirect_stdout
import copy
import io
import json
from pathlib import Path
import runpy
import shutil
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
module = runpy.run_path(str(root / 'bin/check-contracts'))
validate, render, check_runtime, main = [module[name] for name in ('validate', 'render', 'check_runtime', 'main')]


def reject(action, expected):
    try:
        action()
    except ValueError as error:
        assert expected in str(error), str(error)
    else:
        raise AssertionError('Expected rejection: ' + expected)


with tempfile.TemporaryDirectory(prefix='hotbox-contract-drift-') as temp:
    fixture = Path(temp)
    source = fixture / module['SOURCE']
    source.parent.mkdir(parents=True)
    original = json.loads((root / module['SOURCE']).read_text())
    source.write_text(json.dumps(original))
    for group in ('implementation', 'verification'):
        for ref in original[group]:
            target = fixture / ref['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / ref['path'], target)
    data, digest = validate(fixture)
    output = fixture / module['REFERENCE']
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(render(data, digest))
    main.__globals__['ROOT'] = fixture
    sys.argv = ['check-contracts']
    with redirect_stdout(io.StringIO()):
        main()
    output.write_text(output.read_text() + '\nUnreviewed drift\n')
    reject(main, 'reference is stale')
    changed = copy.deepcopy(original)
    changed['operations']['read']['mode'] = 'execute'
    source.write_text(json.dumps(changed))
    reject(lambda: validate(fixture), 'Invalid operation mode')
    changed = copy.deepcopy(original)
    changed['implementation'][0]['symbol'] = 'missing_audited_symbol_12345'
    source.write_text(json.dumps(changed))
    reject(lambda: validate(fixture), 'Missing symbol')
    changed = copy.deepcopy(original)
    changed['node_kinds'].append('not_in_compiled_binary')
    reject(lambda: check_runtime(binary, changed), 'Compiled catalog differs')
print('Contract drift checks passed: stale reference, invalid mode, missing symbol and compiled mismatch rejected.')
