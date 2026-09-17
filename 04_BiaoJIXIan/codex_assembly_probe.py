import json
import ctypes
import sys
import traceback

import NXOpen
import NXOpen.UF


def info_body(body):
    faces = []
    for face in body.GetFaces():
        faces.append({"tag": face.Tag, "is_occurrence": face.IsOccurrence,
                      "prototype": face.Prototype.Tag if face.IsOccurrence else None})
    return {"tag": body.Tag, "is_occurrence": body.IsOccurrence,
            "prototype": body.Prototype.Tag if body.IsOccurrence else None,
            "faces": faces[:4], "face_count": len(faces)}


def info_component(component):
    prototype = component.Prototype
    bodies = []
    if isinstance(prototype, NXOpen.Part):
        if not prototype.IsFullyLoaded:
            load_status = prototype.LoadThisPartFully()
            load_status.Dispose()
        for body in prototype.Bodies:
            occurrence = component.FindOccurrence(body)
            bodies.append({"prototype": info_body(body),
                           "occurrence": info_body(occurrence) if occurrence else None})
    return {"tag": component.Tag, "name": component.Name,
            "prototype_path": prototype.FullPath if prototype else None,
            "fully_loaded": prototype.IsFullyLoaded if prototype else None,
            "bodies": bodies,
            "children": [info_component(child) for child in component.GetChildren()]}


def main():
    source = sys.argv[1]
    destination = sys.argv[2]
    result = {}
    try:
        session = NXOpen.Session.GetSession()
        uf = NXOpen.UF.UFSession.GetUFSession()
        part, status = session.Parts.OpenActiveDisplay(
            source, NXOpen.DisplayPartOption.AllowAdditional)
        status.Dispose()
        root = part.ComponentAssembly.RootComponent
        result = {"part": part.FullPath, "work": session.Parts.Work.FullPath,
                  "root": info_component(root) if root else None}
        first_component = root.GetChildren()[0]
        first_body = first_component.FindOccurrence(next(iter(first_component.Prototype.Bodies)))
        first_face = first_body.GetFaces()[0]
        result["uf_probe"] = {}
        libufun = ctypes.WinDLL("libufun.dll")
        face_body = ctypes.c_uint(0)
        face_body_code = libufun.UF_MODL_ask_face_body(
            ctypes.c_uint(first_face.Tag), ctypes.byref(face_body))
        result["uf_probe"]["c_face_body"] = [face_body_code, face_body.value]
        face_type = ctypes.c_int(0)
        face_type_code = libufun.UF_MODL_ask_face_type(
            ctypes.c_uint(first_face.Tag), ctypes.byref(face_type))
        result["uf_probe"]["c_face_type"] = [face_type_code, face_type.value]
        for name, callback in [
            ("face_body", lambda: uf.Modl.AskFaceBody(first_face.Tag)),
            ("body_faces", lambda: uf.Modl.AskBodyFaces(first_body.Tag)),
            ("face_type", lambda: uf.Modl.AskFaceType(first_face.Tag)),
            ("owning_part", lambda: uf.Obj.AskOwningPart(first_body.Tag)),
            ("prototype", lambda: uf.Assem.AskPrototypeOfOcc(first_body.Tag)),
            ("part_occ", lambda: uf.Assem.AskPartOccurrence(first_body.Tag)),
            ("transform", lambda: uf.Assem.AskTransformOfOcc(first_body.Tag))]:
            try:
                result["uf_probe"][name] = str(callback())
            except Exception as error:
                result["uf_probe"][name] = "ERROR " + str(error)
    except Exception:
        result = {"error": traceback.format_exc()}
    with open(destination, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)


if __name__ == "__main__":
    main()
