"""Read-only NX journal for diagnosing face selection; never changes or saves a part."""
import json
import os
import ctypes as C
from pathlib import Path
import NXOpen
import NXOpen.UF

session = NXOpen.Session.GetSession()
uf = NXOpen.UF.UFSession.GetUFSession()
native = C.WinDLL('libufun.dll')
def face_data(tag):
    typ, sign = C.c_int(), C.c_int()
    point, axis, box = (C.c_double * 3)(), (C.c_double * 3)(), (C.c_double * 6)()
    radius, radius2 = C.c_double(), C.c_double()
    rc = native.UF_MODL_ask_face_data(C.c_uint(tag), C.byref(typ), point, axis, box, C.byref(radius), C.byref(radius2), C.byref(sign))
    if rc: raise RuntimeError('UF_MODL_ask_face_data: ' + str(rc))
    return [typ.value, list(point), list(axis), list(box), radius.value, radius2.value, sign.value]
part = session.Parts.Work
report = {'part': part.FullPath, 'modified': part.IsModified, 'bodies': []}
for body in part.Bodies:
    item = {'tag': int(body.Tag), 'solid': body.IsSolidBody, 'faces': []}
    for face in body.GetFaces():
        record = {'tag': int(face.Tag), 'journal_id': face.JournalIdentifier, 'nx_type': str(face.SolidFaceType)}
        try:
            record['data'] = face_data(face.Tag)
            record['edges'] = [{'tag': int(edge.Tag), 'type': str(edge.SolidEdgeType),
                                'faces': [int(f.Tag) for f in edge.GetFaces()]} for edge in face.GetEdges()]
        except Exception as error:
            record['error'] = str(error)
        item['faces'].append(record)
    report['bodies'].append(item)
Path(os.environ['TEMP'], 'Zhihui-ZeWanMoNi-part.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
