import NXOpen


def main():
    session = NXOpen.Session.GetSession()
    part_path = r"C:\Users\Administrator\Desktop\XT_x_t.prt"
    request_path = r"C:\Temp\codex_direct_test.request"
    dll_path = r"D:\UG智辉钣金插件\application\AutoCreateThreeViews.dll"

    base_part, load_status = session.Parts.OpenBaseDisplay(part_path)
    if load_status is not None:
        load_status.Dispose()
    if base_part is None:
        raise RuntimeError("Failed to open test part")

    session.ExecuteWithStringArguments(dll_path, "ufusr", ["request=" + request_path])


if __name__ == "__main__":
    main()
