"""Install the reviewed, deployed DWG tools in the current user's tool list."""
from pathlib import Path
import datetime
import hashlib
import json
import os

source = Path(__file__).resolve().parents[1]
root = Path(r'D:\UG智辉钣金插件')
catalog = json.loads((source/'tool-library/dwg-20260921/catalog.json').read_text(encoding='utf-8'))
destination = root/'刀图'
legacy = Path(os.environ['APPDATA'])/'Zhihui/ZeWanMoNi/tools'
payload = {}
for tool in catalog['tools']:
    name = tool['file']
    assert Path(name).name == name and name.endswith('.ztool')
    data = (root/'application/ZeWanMoNiTools'/name).read_bytes()
    assert hashlib.sha256(data).hexdigest() == tool['sha256']
    payload[name] = data
destination.mkdir(parents=True, exist_ok=True)
legacy_files = list(legacy.glob('*.ztool')) if legacy.is_dir() else []
old = {name: (destination/name).read_bytes() if (destination/name).exists() else None for name in payload}
backup = root/'backup'/datetime.datetime.now().strftime('before-ZeWanMoNi-user-tools_%Y%m%d_%H%M%S')
backup.mkdir(parents=True, exist_ok=False)
for name, data in old.items():
    if data is not None:
        (backup/name).write_bytes(data)
report = {'destination': str(destination), 'backup': str(backup),
          'created': [n for n, d in old.items() if d is None],
          'preserved': [n for n, d in old.items() if d is not None and d != payload[n]],
          'migrated': [],
          'tools': {t['file']: t['sha256'] for t in catalog['tools']}}
(backup/'installation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
changed = []
try:
    for name, data in payload.items():
        target = destination/name
        assert (target.read_bytes() if target.exists() else None) == old[name], 'File changed concurrently'
        if old[name] is None:
            target.write_bytes(data)
            changed.append(name)
    for name, data in payload.items():
        assert (destination/name).read_bytes() == (old[name] if old[name] is not None else data)
    for previous in legacy_files:
        target = destination/previous.name
        if not target.exists():
            target.write_bytes(previous.read_bytes())
            changed.append(previous.name)
            report['migrated'].append(previous.name)
    original = destination/'原图'/'折弯模拟.dwg'
    if not original.exists():
        original.parent.mkdir(parents=True, exist_ok=True)
        original.write_bytes((source/'tool-library/dwg-20260921/source.dwg').read_bytes())
        report['source_dwg'] = str(original)
except Exception:
    for name in reversed(changed):
        if name not in old or old[name] is None:
            (destination/name).unlink()
        else:
            (destination/name).write_bytes(old[name])
    raise
(source/'build-codex/tool-library-installation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=True, indent=2))
