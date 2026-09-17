import json
import os
import sys
import traceback

import NXOpen


def main():
    assembly_path = sys.argv[1]
    output_path = sys.argv[2]
    dll_path = sys.argv[3]
    mode = sys.argv[4] if len(sys.argv) > 4 else "autotest_bottom"
    result = {}
    try:
        session = NXOpen.Session.GetSession()
        assembly, status = session.Parts.OpenActiveDisplay(
            assembly_path, NXOpen.DisplayPartOption.AllowAdditional)
        status.Dispose()
        children = assembly.ComponentAssembly.RootComponent.GetChildren()
        for component in children:
            part = component.Prototype
            if not part.IsFullyLoaded:
                load_status = part.LoadThisPartFully()
                load_status.Dispose()
        def count_features():
            return {part.FullPath: sum(len(body.GetFeatures()) for body in part.Bodies)
                    for part in {component.Prototype for component in children}}
        result["before"] = count_features()
        if mode == "autotest_bottom":
            os.environ["BIAOJIXIAN_AUTOTEST_BOTTOM"] = "1"
        session.ExecuteWithStringArguments(dll_path, "ufusr", [mode])
        result["after"] = count_features()
        result["work_after"] = session.Parts.Work.FullPath
        result["display_after"] = session.Parts.Display.FullPath
        for component in children:
            part = component.Prototype
            if result["after"][part.FullPath] > result["before"][part.FullPath]:
                part.Save(NXOpen.BasePart.SaveComponents.FalseValue,
                          NXOpen.BasePart.CloseAfterSave.FalseValue)
    except Exception:
        result["error"] = traceback.format_exc()
    with open(output_path, "w", encoding="utf-8") as output:
        json.dump(result, output, ensure_ascii=False, indent=2)


if __name__ == "__main__":
    main()
