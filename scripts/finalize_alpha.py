"""Add game-owned Windows alpha launch surfaces to the shared release stage."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import tomllib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "psxrecomp" / "tools"))
from create_release_zip import create_release_zip


def revision(path):
    return subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--artifact", default="windows-x64")
    args = parser.parse_args()
    if not re.fullmatch(r"windows-x64(?:-[A-Za-z0-9][A-Za-z0-9.-]*)?", args.artifact):
        parser.error("Expected a Windows x64 artifact label")
    stage = (ROOT / "dist" / ("stage-game-" + args.artifact)).resolve()
    if not stage.is_relative_to((ROOT / "dist").resolve()) or not stage.is_dir():
        raise RuntimeError("Expected the shared packager's Windows release stage")
    cache = (args.build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
    if "PSX_OPENXR:BOOL=ON" not in cache:
        raise RuntimeError("The Windows alpha must be built with OpenXR support")
    if "PSX_DEBUG_TOOLS:BOOL=ON" not in cache:
        raise RuntimeError("The alpha launcher requires TCP startup inspection")
    if "PSXRECOMP_BIOS_STEMS:STRING=OpenBIOS\n" not in cache:
        raise RuntimeError("The alpha must link only the bundled OpenBIOS backend")
    if (stage / "psx_game_version.txt").read_text().strip() != args.version:
        raise RuntimeError("Release version does not match the executable build stamp")
    for name in ("RunFlat.bat", "RunVR-VDXR.bat", "RunVR-SteamVR.bat", "RunVR-Oculus.bat",
                 "ALPHA_README.md", "vr/run_vr.ps1"):
        target = stage / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, target)
    config = stage / "game.toml"
    text = config.read_text(encoding="utf-8")
    text, count = re.subn(r'(?ms)(^\[game\]\s*\n.*?^disc\s*=\s*)"[^\n]*"', r'\1""', text, count=1)
    if count != 1:
        raise RuntimeError("Could not remove the developer's disc path from game.toml")
    config.write_text(text, encoding="utf-8", newline="\n")
    parsed = tomllib.loads(text)
    if parsed["game"]["disc"] or not parsed["runtime"]["overlay_cache"]:
        raise RuntimeError("Invalid portable release configuration")
    manifests = list((stage / "mods/bundled/moh.vr.stereo").glob("*/manifest.toml"))
    if len(manifests) != 1:
        raise RuntimeError("The release must include the game's VR mod")
    feature = tomllib.loads(manifests[0].read_text())["feature"][0]
    if feature["channel"] != "experimental" or not feature["default_enabled"]:
        raise RuntimeError("VR support must survive release filtering and register its runtime hooks")
    exe = stage / "Medal_of_Honor__Recompiled.exe"
    with exe.open("rb") as source:
        exe_hash = hashlib.file_digest(source, "sha256").hexdigest()
    info = {
        "version": args.version,
        "channel": "alpha",
        "platform": "windows-x64",
        "artifact": args.artifact,
        "game_commit": revision(ROOT),
        "framework_commit": revision(ROOT / "psxrecomp"),
        "ui_commit": revision(ROOT / "recomp-ui"),
        "openxr": True,
        "bios_backends": "OpenBIOS",
        "exe_sha256": exe_hash,
    }
    (stage / "BUILD_INFO.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    (stage / "README.txt").write_text(
        "Medal of Honor Recompiled " + args.version + " — Native VR Alpha\n"
        "Flat: RunFlat.bat. VR: connect headset/OpenXR runtime, then run the\n"
        "launcher for that runtime (RunVR-VDXR.bat / RunVR-SteamVR.bat / RunVR-Oculus.bat).\n"
        "Select your own SLUS-00974 CUE/BIN image. No game data is included.\n"
        "See ALPHA_README.md for setup, controls and known limitations.\n",
        encoding="utf-8",
    )
    output = ROOT / "dist" / f"moh-{args.version}-{args.artifact}.zip"
    # Antivirus may briefly hold the ZIP produced by the shared packager.
    for attempt in range(3):
        try:
            create_release_zip(stage, output)
            break
        except PermissionError:
            if attempt == 2:
                raise
            time.sleep(.5)
    print(output)


if __name__ == "__main__":
    main()
