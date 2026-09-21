"""Read a DWG copy through installed AutoCAD ObjectDBX; never save the source.

Requires pywin32 and a running AutoCAD. The JSON preserves source handles,
block insertion transforms and analytic arcs for repeatable profile extraction.
"""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import shutil
import win32com.client


def entity(e):
    result = {'handle': e.Handle, 'type': e.ObjectName, 'layer': e.Layer}
    properties = {
        'AcDbLine': ['StartPoint', 'EndPoint'],
        'AcDbArc': ['Center', 'Radius', 'StartAngle', 'EndAngle', 'Normal'],
        'AcDbCircle': ['Center', 'Radius', 'Normal'],
        'AcDbBlockReference': ['Name', 'InsertionPoint', 'Rotation', 'XScaleFactor', 'YScaleFactor', 'ZScaleFactor', 'Normal'],
        'AcDbText': ['TextString', 'InsertionPoint', 'Height', 'Rotation'],
        'AcDbMText': ['TextString', 'InsertionPoint', 'Height', 'Rotation'],
        'AcDbSolid': ['Coordinates'],
        'AcDbPolyline': ['Coordinates', 'Closed', 'Elevation', 'Normal'],
        'AcDb2dPolyline': ['Coordinates', 'Closed', 'Normal'],
        'AcDbLeader': ['Coordinates'],
        'AcDbRotatedDimension': ['Measurement', 'TextOverride', 'TextPosition', 'LinearScaleFactor'],
        'AcDbAlignedDimension': ['Measurement', 'TextOverride', 'TextPosition', 'LinearScaleFactor'],
    }
    for key in properties.get(e.ObjectName, []):
        result[key] = getattr(e, key)
    if e.ObjectName == 'AcDbPolyline':
        result['bulges'] = [e.GetBulge(i) for i in range(len(result['Coordinates']) // 2)]
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('dwg', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    data = args.dwg.read_bytes()
    source_hash = hashlib.sha256(data).hexdigest()
    snapshot = args.output / 'source.dwg'
    shutil.copyfile(args.dwg, snapshot)
    app = win32com.client.GetActiveObject('AutoCAD.Application')
    version = app.Version.split('.')[0]
    db = app.GetInterfaceObject('ObjectDBX.AxDbDocument.' + version)
    db.Open(str(snapshot.resolve()))
    variables = {}
    for doc in app.Documents:
        if doc.FullName.casefold() == str(args.dwg.resolve()).casefold():
            variables = {v: doc.GetVariable(v) for v in ['INSUNITS', 'MEASUREMENT', 'LUNITS', 'DIMLFAC']}
    blocks = {}
    for block in db.Blocks:
        if block.Name.startswith('*'):
            continue
        blocks[block.Name] = {'origin': list(block.Origin), 'entities': [entity(e) for e in block]}
    result = {'source': str(args.dwg.resolve()), 'sha256': source_hash,
              'autocad_version': app.Version, 'active_drawing_variables': variables,
              'model': [entity(e) for e in db.ModelSpace], 'blocks': blocks}
    (args.output / 'entities.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    assert hashlib.sha256(args.dwg.read_bytes()).hexdigest() == source_hash
    print(json.dumps({'source_unchanged': True, 'model': len(result['model']),
                      'blocks': {name: dict(collections.Counter(e['type'] for e in block['entities'])) for name, block in blocks.items()},
                      'variables': variables}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
