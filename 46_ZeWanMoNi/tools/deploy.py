"""Deploy only the reviewed Release artifacts; preserve licensing and manifests."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil
import struct
import xml.etree.ElementTree as ET

source = Path(__file__).resolve().parents[1]
repo = source.parents[1]
root = Path(r'D:\UG智辉钣金插件')
build = source / 'build-codex/Release'
def digest(data):
    return hashlib.sha256(data).hexdigest().upper()

def verify_pe(data, ui=True):
    assert data[:2] == b'MZ'
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0'
    checksum = struct.unpack_from('<I', data, pe+88)[0]
    flags = struct.unpack_from('<H', data, pe+94)[0]
    assert checksum and flags & 0x40 and flags & 0x100
    # Recalculate the PE checksum, treating its own DWORD as zero.
    total = 0
    for i in range(0, len(data), 2):
        word = 0 if pe+88 <= i < pe+92 else int.from_bytes(data[i:i+2], 'little')
        total += word
        total = (total & 0xffff) + (total >> 16)
    total = ((total & 0xffff) + (total >> 16)) + len(data)
    assert total == checksum, 'Invalid PE checksum'
    for name in [b'ufusr\0', b'ufusr_ask_unload\0'] + ([b'ZfnxEnsureAuthorized',b'ufusr_cleanup\0'] if ui else []):
        assert name in data, name
    if ui:
        assert b'Z\0H\0I\0H\0U\0I\0.\0C\0H\0A\0I\0J\0I\0J\0I\0A\0' in data
    return {'checksum': checksum, 'ASLR': True, 'DEP': True, 'licenseGate': ui}

def add_menu(data, ribbon=False):
    text = data.decode('gb18030')
    if 'BUTTON  ZeWanMoNi' in text:
        return data
    newline = '\r\n' if '\r\n' in text else '\n'
    block = [' BUTTON  ZeWanMoNi', ' LABEL   折弯模拟']
    if not ribbon:
        block += [' BITMAP  ZeWanMoNi.bmp', ' ACTIONS ZeWanMoNi']
    anchor = ' BUTTON  StandardPartsLibrary'
    assert text.count(anchor) == 1
    return text.replace(anchor, newline.join(block)+newline+newline+anchor).encode('gb18030')

names = ['ZeWanMoNi.dll', 'ZeWanMoNi.dlx', 'ZeWanMoNi.bmp', 'ZeWanMoNiExample.ztool']
payload = {'application/'+name: (build/name).read_bytes() for name in names}
catalog = json.loads((source/'tool-library/dwg-20260921/catalog.json').read_text(encoding='utf-8'))
for tool in catalog['tools']:
    data = (build/'ZeWanMoNiTools'/tool['file']).read_bytes()
    assert digest(data) == tool['sha256'].upper(), 'Tool library does not match reviewed catalog'
    payload['application/ZeWanMoNiTools/'+tool['file']] = data
protection = {name:verify_pe(payload['application/'+name],name=='ZeWanMoNi.dll') for name in names if name.endswith('.dll')}
ns = {'v':'http://schemas.microsoft.com/developer/msbuild/2003'}
for target in ['ZeWanMoNi']:
    project = ET.parse(source/('build-codex/'+target+'.vcxproj'))
    release = next(g for g in project.findall('v:ItemDefinitionGroup',ns) if "=='Release|x64'" in g.get('Condition',''))
    assert 'ZH_PROTECTED_BUILD=1' in release.findtext('v:ClCompile/v:PreprocessorDefinitions',namespaces=ns)
    assert 'NDEBUG' in release.findtext('v:ClCompile/v:PreprocessorDefinitions',namespaces=ns)
    assert release.findtext('v:ClCompile/v:Optimization',namespaces=ns) == 'MaxSpeed'
    for flag in ['SetChecksum','DataExecutionPrevention','RandomizedBaseAddress']:
        assert release.findtext('v:Link/v:'+flag,namespaces=ns) == 'true'
assert (root/'application/ZhaoFuNxLicenseGate.dll').is_file()
manifest_paths = [root/'manifest/file-hashes.json', root/'manifest/zhihui-package.json']
old_manifests = {p: p.read_bytes() for p in manifest_paths}
hashes, package = [json.loads(old_manifests[p].decode('utf-8-sig')) for p in manifest_paths]
assert package['requiresZhaoFuGate'] and package['tamperValidation'] == 'pe-checksum'
for name in ['UGZH_design.men','UGZH_design.rtb']:
    path = root/'startup'/name
    payload['startup/'+name] = add_menu(path.read_bytes(), name.endswith('.rtb'))
for rel, data in payload.items():
    entries = [e for e in hashes if e['path'] == rel]
    assert len(entries) <= 1
    entry = {'path': rel, 'sha256': digest(data), 'bytes': len(data)}
    if entries:
        entries[0].update(entry)
    else:
        hashes.append(entry)
command = {
    'launcherName': 'ZeWanMoNi', 'nativeDll': 'ZeWanMoNi.dll',
    'featureCode': 'ZHIHUI.CHAIJIJIA', 'displayName': '折弯模拟',
    'entryPoint': 'ufusr', 'authorizationGate': 'native-multi-entry',
    'menuButton': 'ZeWanMoNi', 'actionsName': 'ZeWanMoNi',
    'exportsVerified': True, 'sha256': digest(payload['application/ZeWanMoNi.dll']),
    'dlxFiles': 'application/ZeWanMoNi.dlx', 'iconFiles': 'application/ZeWanMoNi.bmp',
    'runtimeFiles': ['application/ZeWanMoNiExample.ztool'] + ['application/ZeWanMoNiTools/'+tool['file'] for tool in catalog['tools']],
}
existing = [c for c in package['commands'] if c.get('launcherName') == 'ZeWanMoNi']
assert len(existing) <= 1
if existing:
    existing[0].update(command)
else:
    package['commands'].append(command)
backup = root/'backup'/datetime.datetime.now().strftime('before-ZeWanMoNi_%Y%m%d_%H%M%S')
backup.mkdir(parents=True, exist_ok=False)
old_files = {rel: (root/rel).read_bytes() if (root/rel).exists() else None for rel in payload}
for rel, data in old_files.items():
    if data is not None:
        dest = backup/rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
for p, data in old_manifests.items():
    dest = backup/'manifest'/p.name
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
(backup/'created-files.json').write_text(json.dumps([r for r,d in old_files.items() if d is None],indent=2),encoding='utf-8')
changed = []
written_manifests = []
try:
    for p, data in old_manifests.items():
        assert p.read_bytes() == data, 'Manifest changed concurrently'
    for rel,data in payload.items():
        target = root/rel
        assert target.resolve().is_relative_to(root.resolve())
        assert (target.read_bytes() if target.exists() else None) == old_files[rel], 'Artifact changed concurrently'
        if data != old_files[rel]:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            changed.append(rel)
    for p, value in zip(manifest_paths, [hashes,package]):
        assert p.read_bytes() == old_manifests[p], 'Manifest changed concurrently'
        p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        written_manifests.append(p)
    installed_hashes = json.loads(manifest_paths[0].read_text(encoding='utf-8'))
    for rel,data in payload.items():
        assert (root/rel).read_bytes() == data
        actual = next(e for e in installed_hashes if e['path']==rel)
        assert actual['sha256'] == digest(data) and actual['bytes'] == len(data)
    verify_pe((root/'application/ZeWanMoNi.dll').read_bytes())
    actual_package = json.loads(manifest_paths[1].read_text(encoding='utf-8'))
    assert next(c for c in actual_package['commands'] if c.get('launcherName')=='ZeWanMoNi')['sha256'] == command['sha256']
except Exception:
    for rel in reversed(changed):
        if old_files[rel] is None:
            (root/rel).unlink()
        else:
            (root/rel).write_bytes(old_files[rel])
    for p in written_manifests:
        p.write_bytes(old_manifests[p])
    raise

# Source menu references track the added command without replacing other work.
for name in ['UGZH_design.men','UGZH_design.rtb']:
    p = repo/'deployment_reference/startup'/name
    p.write_bytes(add_menu(p.read_bytes(),name.endswith('.rtb')))
p = repo/'deployment_reference/zhihui-package.json'
raw = p.read_text(encoding='utf-8-sig')
decoder = json.JSONDecoder()
start = raw.index('[',raw.index('"commands"'))
commands, end = decoder.raw_decode(raw,start)
end -= 1  # commands array closing bracket
position = start+1
replaced = False
while position < end:
    while position < end and (raw[position].isspace() or raw[position]==','):
        position += 1
    if position >= end:
        break
    value, after = decoder.raw_decode(raw,position)
    if value.get('launcherName') == 'ZeWanMoNi':
        raw = raw[:position]+json.dumps(command,ensure_ascii=False,indent=4)+raw[after:]
        replaced = True
        break
    position = after
if not replaced:
    prefix = raw[:end].rstrip()
    raw = prefix+',\n'+json.dumps(command,ensure_ascii=False,indent=4)+'\n'+raw[end:]
assert json.loads(raw)['commands'][-1].get('launcherName')=='ZeWanMoNi' or replaced
p.write_text(raw,encoding='utf-8')
report={'backup':str(backup),'protection':protection,'artifacts':{r:{'sha256':digest(d),'bytes':len(d)} for r,d in payload.items()}}
(source/'build-codex/deployment-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=True,indent=2))
