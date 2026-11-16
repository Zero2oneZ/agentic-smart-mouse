#!/usr/bin/env python3
"""Verify user installation and SQLite WAL backups without touching the real desktop."""
import os
from contextlib import closing
from pathlib import Path
import shutil
import sqlite3
import subprocess
import tempfile

source=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='hotbox-install-') as temp:
    root=Path(temp)/'project'
    home=Path(temp)/'home'
    for name in ('bin','build','pkgbuild'):(root/name).mkdir(parents=True)
    shutil.copy2(source/'bin/install-local',root/'bin/install-local')
    build=root/'bin/build';build.write_text('#!/bin/sh\nexit 0\n');build.chmod(0o755)
    for name in ('hotbox','hotbox-core'):(root/'build'/name).write_text('new '+name)
    for name in ('hotbox.desktop','hotbox.svg'):shutil.copy2(source/'pkgbuild'/name,root/'pkgbuild'/name)
    data=home/'.local/share/hotbox';data.mkdir(parents=True)
    binaries=home/'.local/bin';binaries.mkdir(parents=True)
    for name in ('hotbox','hotbox-core'):(binaries/name).write_text('old '+name)
    # Keep the source connection open so committed data remains in its WAL.
    database=sqlite3.connect(data/'hotbox.sqlite3')
    database.execute('pragma journal_mode=wal')
    database.execute('create table evidence(value text)')
    database.execute("insert into evidence values('preserved')");database.commit()
    env=os.environ.copy();env['HOME']=str(home);env['XDG_DATA_HOME']=str(home/'.local/share')
    subprocess.run([str(root/'bin/install-local'),'--desktop-only'],env=env,check=True,capture_output=True,text=True)
    backups=list((data/'backups').glob('*hotbox.sqlite3'));assert len(backups)==1
    with closing(sqlite3.connect(backups[0])) as backup:assert backup.execute('select value from evidence').fetchone()==('preserved',)
    database.close()
    for name in ('hotbox','hotbox-core'):assert (binaries/name).read_text()=='new '+name
    assert (home/'.local/share/applications/hotbox.desktop').exists()
    assert (home/'.local/share/icons/hicolor/scalable/apps/hotbox.svg').exists()
    assert len(list((data/'backups').iterdir()))==3
print('Local install passed: live WAL backup, binary rollback copies, paired binaries, launcher and icon.')
