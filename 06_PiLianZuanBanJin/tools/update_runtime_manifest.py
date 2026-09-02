from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


TRACKED_PATHS = (
    "application/Write_Prat_Attr.dll",
    "application/Write_Prat_AttrHelp/index.html",
    "application/Write_Prat_AttrHelp/index-v20260730-1.html",
    "application/Write_Prat_AttrHelp/dialog-real.png",
    "application/Write_Prat_AttrHelp/config-real.png",
    "application/Write_Prat_AttrHelp/assembly-auto.png",
    "application/Write_Prat_AttrHelp/part-auto.png",
    "application/Write_Prat_AttrHelp/Write_Prat_Attr.map",
    "application/PiLianZuanBanJin.dll",
    "application/PiLianZuanBanJinHelp/index.html",
    "application/PiLianZuanBanJinHelp/index-v20260730-2.html",
    "application/PiLianZuanBanJinHelp/dialog-real.png",
    "application/PiLianZuanBanJinHelp/assembly-real.png",
    "application/PiLianZuanBanJinHelp/rules-real.png",
    "application/PiLianZuanBanJinHelp/manual-base-real.png",
    "application/PiLianZuanBanJinHelp/PiLianZuanBanJin.map",
)

EXTRA_HELP_RUNTIME_PATHS = (
    "application/TwoPointSiBian.dlx",
    "application/CaiPinBan.dlx",
    "application/CaiRBan.dlx",
    "application/CaiR1.dlx",
    "application/TiaoZenBanLeiCiCun.dlx",
    "application/AutoCreateThreeViewsUI/AutoCreateThreeViewsUI.exe",
    "application/MinXiBiaoUI/MinXiBiaoUI.exe",
)


def flatten_entries(value):
    if isinstance(value, list):
        for item in value:
            yield from flatten_entries(item)
    elif (
        isinstance(value, dict)
        and isinstance(value.get("value"), list)
        and set(value).issubset({"value", "Count"})
    ):
        # Windows PowerShell 5 may serialize a wrapped collection as
        # {"value": [...], "Count": N}. Recover the original entries.
        yield from flatten_entries(value["value"])
    else:
        yield value


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("runtime_root", type=Path)
    args = parser.parse_args()

    root = args.runtime_root.resolve()
    hash_path = root / "manifest" / "file-hashes.json"
    package_path = root / "manifest" / "zhihui-package.json"
    package_data = json.loads(package_path.read_text(encoding="utf-8-sig"))
    package_dlls = {
        command["launcherName"]: f'application/{command["nativeDll"]}'
        for command in package_data["commands"]
    }
    tracked_paths = set(TRACKED_PATHS)
    tracked_paths.update(EXTRA_HELP_RUNTIME_PATHS)
    tracked_paths.update(package_dlls.values())
    help_root = root / "application" / "ZhihuiHelp"
    if help_root.is_dir():
        tracked_paths.update(
            path.relative_to(root).as_posix()
            for path in help_root.rglob("*")
            if path.is_file()
        )

    raw_entries = json.loads(hash_path.read_text(encoding="utf-8-sig"))
    entries = [
        entry
        for entry in flatten_entries(raw_entries)
        if isinstance(entry, dict)
        and entry.get("path")
        and str(entry.get("path", "")).replace("\\", "/") not in tracked_paths
    ]

    for relative in sorted(tracked_paths):
        absolute = root / Path(relative.replace("/", "\\"))
        entries.append(
            {
                "path": relative,
                "sha256": sha256(absolute),
                "bytes": absolute.stat().st_size,
            }
        )

    entries.sort(key=lambda entry: str(entry.get("path", "")).lower())
    paths = [str(entry.get("path", "")) for entry in entries]
    if len(paths) != len(set(paths)):
        raise RuntimeError("file-hashes.json still contains duplicate paths")
    hash_path.write_text(
        json.dumps(entries, ensure_ascii=False, indent=4) + "\n",
        encoding="utf-8",
    )

    package_text = package_path.read_text(encoding="utf-8-sig")
    package_hashes = {}
    for launcher_name, relative_dll in package_dlls.items():
        dll_hash = sha256(root / Path(relative_dll.replace("/", "\\")))
        pattern = re.compile(
            r'("launcherName"\s*:\s*"' + re.escape(launcher_name) + r'"[\s\S]*?'
            r'"sha256"\s*:\s*")[0-9A-Fa-f]+(")'
        )
        package_text, count = pattern.subn(
            lambda match, value=dll_hash: match.group(1) + value + match.group(2),
            package_text,
            count=1,
        )
        if count != 1:
            raise RuntimeError(f"{launcher_name} package hash field was not found")
        package_hashes[launcher_name] = dll_hash
    package_path.write_text(package_text, encoding="utf-8")

    reloaded = json.loads(hash_path.read_text(encoding="utf-8"))
    if not isinstance(reloaded, list) or any(isinstance(item, list) for item in reloaded):
        raise RuntimeError("file-hashes.json is not a flat list")
    tracked = {entry["path"]: entry for entry in reloaded if entry["path"] in tracked_paths}
    if set(tracked) != set(tracked_paths):
        raise RuntimeError("tracked runtime entries are incomplete")
    for relative, entry in tracked.items():
        absolute = root / Path(relative.replace("/", "\\"))
        if entry["sha256"] != sha256(absolute) or entry["bytes"] != absolute.stat().st_size:
            raise RuntimeError(f"manifest mismatch: {relative}")

    print(
        json.dumps(
            {
                "entry_count": len(reloaded),
                "tracked_count": len(tracked),
                "dll_sha256": package_hashes["PiLianZuanBanJin"],
                "package_dll_sha256": package_hashes,
                "flat": True,
                "duplicates": False,
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
