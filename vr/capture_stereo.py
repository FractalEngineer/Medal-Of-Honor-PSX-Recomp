"""Load a verified save slot, capture complete eye pairs and 96-frame fingerprints.

Run against a windowed diagnostic process configured before launch. This does
not enable stereo, change offsets, or advance input. Requires the paired TCP API.
"""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "psxrecomp" / "tools"))
from debug_client import connect, send_cmd


def command(name, **kwargs):
    with connect(timeout=20) as sock:
        result = send_cmd(sock, {"cmd": name, **kwargs})
    if not result.get("ok"):
        raise RuntimeError(result)
    return result


def save(path, name, result):
    (path / name).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--slot", type=int, default=3)
    parser.add_argument("--pairs", type=int, default=2)
    parser.add_argument("--executable", type=Path, help="Record built binary and actual CMake framework root")
    args = parser.parse_args()
    directory = args.directory.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    if any(p.stem[1:].isdigit() for p in directory.glob("p*.json")):
        raise FileExistsError("Use a fresh directory for each capture run")
    save(directory, "launch_config.json", {
        "environment": {k:v for k,v in os.environ.items()
            if k.startswith("PSX_VR_") or k in ("PSX_OPENXR","PSX_RENDER_PASS_VERIFY")},
        "game_revision": subprocess.check_output(["git","rev-parse","HEAD"], cwd=ROOT,text=True).strip(),
        "framework_pin": subprocess.check_output(["git","rev-parse","HEAD"], cwd=ROOT/"psxrecomp",text=True).strip(),
        "note": "Environment inherited by capture client; launch must use the same environment."
    })
    if args.executable:
        executable = args.executable.resolve()
        provenance = {"executable": str(executable),
            "sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
        cache = executable.parent / "CMakeCache.txt"
        if cache.exists():
            prefix = "PSXRECOMP_ROOT:PATH="
            roots = [line[len(prefix):] for line in cache.read_text(encoding="utf-8-sig").splitlines()
                     if line.startswith(prefix)]
            if roots:
                framework = Path(roots[0])
                provenance["framework_root"] = str(framework)
                provenance["framework_revision"] = subprocess.check_output(
                    ["git","rev-parse","HEAD"],cwd=framework,text=True).strip()
                provenance["framework_tracked_changes"] = subprocess.check_output(
                    ["git","status","--porcelain","--untracked-files=no"],cwd=framework,text=True).splitlines()
        save(directory,"build_provenance.json",provenance)
    save_file = ROOT / "saves" / "openbios" / f"state_8001DFD4_slot{args.slot:02d}.pst"
    if save_file.exists():
        save(directory, "save_provenance.json", {"slot": args.slot,
             "file": str(save_file.relative_to(ROOT)), "size": save_file.stat().st_size,
             "mtime_ns": save_file.stat().st_mtime_ns,
             "sha256": hashlib.sha256(save_file.read_bytes()).hexdigest()})
    # Fresh windowed processes may take seconds to finish startup/link caches.
    deadline = time.monotonic() + 30
    while True:
        try:
            before = command("savestate_status")
            break
        except (ConnectionError, OSError):
            if time.monotonic() > deadline: raise
            time.sleep(.2)
    save(directory, "fingerprint_arm.json", command("frame_fingerprint", reset_on_load=1))
    save(directory, "load_request.json", command("savestate", slot=args.slot, op="load"))
    deadline = time.monotonic() + 30
    while True:
        status = command("savestate_status")
        if status["generation"] > before["generation"] and not status["pending"]:
            if not status["last_ok"] or status["last_slot"] != args.slot:
                raise RuntimeError(status)
            save(directory, "load_completed.json", status)
            break
        if time.monotonic() > deadline:
            raise TimeoutError("save load did not complete")
        time.sleep(.1)
    if args.pairs:
        save(directory, "dump_arm.json", command("stereo_dump", path=directory.as_posix(), count=args.pairs))
    deadline = time.monotonic() + 45
    while True:
        fingerprint = command("frame_fingerprint", count=96)
        manifests = [p for p in directory.glob("p*.json") if p.stem[1:].isdigit()]
        if fingerprint["available"] >= 96 and len(manifests) >= args.pairs:
            save(directory, "fingerprint.json", fingerprint)
            break
        if time.monotonic() > deadline:
            raise TimeoutError("insufficient complete pairs or post-load frames")
        time.sleep(.2)
    for cmd in ("stereo_stats", "render_pass_stats", "gl_interp", "video_info"):
        save(directory, cmd + ".json", command(cmd))
    if os.environ.get("PSX_VR_OPENXR") == "1":
        for cmd in ("openxr_stats", "openxr_views"):
            save(directory, cmd + ".json", command(cmd))
    shot = command("present_shot_seq")
    save(directory, "present_arm.json", command("present_shot", path=(directory / "presented.png").as_posix()))
    deadline = time.monotonic() + 10
    while True:
        completed = command("present_shot_seq")
        if completed["seq"] > shot["seq"]:
            save(directory, "present_completed.json", completed)
            if not completed["wrote"]:
                raise RuntimeError("present readback did not write PNG")
            break
        if time.monotonic() > deadline:
            raise TimeoutError("present readback did not complete")
        time.sleep(.1)
    print(f"Verified slot {args.slot}; {len(manifests)} complete pairs; 96-frame receipt: {directory}")


if __name__ == "__main__":
    main()
