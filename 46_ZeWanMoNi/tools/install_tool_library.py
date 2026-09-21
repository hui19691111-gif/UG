"""Install the reviewed, deployed DWG tools in the current user's tool list."""
from pathlib import Path
import datetime
import hashlib
import json
import os

source = Path(__file__).resolve().parents[1]
root = Path(r'D:\UG智辉钣金插件')
catalog = json.loads((source/'tool-library/dwg-20260921/catalog.json').read_text(encoding='utf-8'))
destination = Path(os.environ['APPDATA'])/'Zhihui/ZeWanMoNi/tools'
payload = {}
for tool in catalog['tools']:
    name = tool['file']
    assert Path(name).name == name and name.endswith('.ztool')
    data = (root/'application/ZeWanMoNiTools'/name).read_bytes()
    assert hashlib.sha256(data).hexdigest() == tool['sha256']
    payload[name] = data
destination.mkdir(parents=True, exist_ok=True)
old = {name: (destination/name).read_bytes() if (destination/name).exists() else None for name in payload}
backup = root/'backup'/datetime.datetime.now().strftime('before-ZeWanMoNi-user-tools_%Y%m%d_%H%M%S')
backup.mkdir(parents=True, exist_ok=False)
for name, data in old.items():
    if data is not None:
        (backup/name).write_bytes(data)
report = {'destination': str(destination), 'backup': str(backup),
          'created': [n for n, d in old.items() if d is None],
          'replaced': [n for n, d in old.items() if d is not None and d != payload[n]],
          'tools': {t['file']: t['sha256'] for t in catalog['tools']}}
(backup/'installation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
changed = []
try:
    for name, data in payload.items():
        target = destination/name
        assert (target.read_bytes() if target.exists() else None) == old[name], 'File changed concurrently'
        if old[name] != data:
            target.write_bytes(data)
            changed.append(name)
    for name, data in payload.items():
        assert (destination/name).read_bytes() == data
except Exception:
    for name in reversed(changed):
        if old[name] is None:
            (destination/name).unlink()
        else:
            (destination/name).write_bytes(old[name])
    raise
(source/'build-codex/tool-library-installation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=True, indent=2))
