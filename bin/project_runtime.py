"""Shared project paths and non-executing private credential reader."""
import os
from pathlib import Path
import stat

ROOT = Path(__file__).resolve().parent.parent
DATA = Path(os.environ.get('HOTBOX_DATA_DIR', Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'hotbox'))


def environment():
    env = os.environ.copy()
    env['HOTBOX_PROJECT_ROOT'] = str(ROOT)
    env['HOTBOX_DATA_DIR'] = str(DATA)
    env['PATH'] = os.pathsep.join([str(ROOT / '.tools/ollama/bin'), str(ROOT / '.tools/rust/bin'), str(ROOT / 'bin'), str(Path.home() / '.local/bin'), env.get('PATH', '')])
    env.setdefault('CARGO_HOME', str(ROOT / '.tools/cargo'))
    return env


def cloud_key():
    if 'OLLAMA_API_KEY' in os.environ:
        return os.environ['OLLAMA_API_KEY']
    path = DATA / 'secrets.env'
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    except OSError:
        return ''
    with os.fdopen(fd, 'r') as file:
        info = os.fstat(file.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077 or info.st_size > 65536:
            return ''
        result = ''
        for line in file:
            line = line.strip()
            if line.startswith('export '):
                line = line[7:].strip()
            name, separator, value = line.partition('=')
            if not separator or name.strip() != 'OLLAMA_API_KEY':
                continue
            value = value.strip()
            if value.startswith(('"', "'")):
                if len(value) < 2 or value[-1] != value[0]:
                    return ''
                value = value[1:-1]
            if any(c in value for c in ('\r', '\x00', '"', "'")):
                return ''
            result = value
        return result
