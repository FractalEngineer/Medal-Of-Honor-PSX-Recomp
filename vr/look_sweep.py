"""Scripted look sweep, headless, comparing the route against the baseline per pose.

The reported headset defects (empty tiles when looking down, near-player floor gaps) are
view-direction dependent, so driving the in-game look axes reproduces the conditions without a
headset: same scripted input, same save, two backends, one screenshot per pose. Any per-pose
difference between the two runs is the route's doing; identical screenshots mean the defects are
pre-existing in the baseline.

Use check_movement.load's pause-aware path so a paused save actually reaches gameplay.

Usage: vr_sweep.py <tag> [interp] [slot]
"""
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(r"C:\Users\titan\Desktop\Github_Projects\Mine\Medal-Of-Honor-PSX-Recomp")
sys.path.insert(0, str(ROOT / "psxrecomp" / "tools"))
sys.path.insert(0, str(ROOT / "vr"))
from debug_client import connect, send_cmd  # noqa: E402

TAG = sys.argv[1]
INTERP = "interp" in sys.argv[2:]
SLOT = int(next((a for a in sys.argv[2:] if a.isdigit()), 3))
OUT = ROOT / "analysis" / "vr-proof" / TAG
OUT.mkdir(parents=True, exist_ok=True)

CASES = [
    ("level", {}),
    ("bob-down-a", {"ry": 400}),
    ("bob-down-b", {"ry": 700}),
    ("look-down", {"ry": 1000}),
    ("look-up", {"ry": -700}),
    ("turn-left", {"rx": -600}),
    ("turn-right", {"rx": 600}),
    ("bob-mix", {"rx": 300, "ry": 300, "ly": 700}),
    ("level-again", {}),
]


def stop_game():
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Stop-Process -Name Medal_of_Honor__Recompiled,vrserver,"
                    "vrmonitor,vrdashboard -Force -ErrorAction SilentlyContinue"],
                   capture_output=True)
    time.sleep(3)


def cmd(name, timeout=90.0, **kw):
    with connect(timeout=timeout) as sock:
        return send_cmd(sock, {"cmd": name, **kw})


def wait_frames(n):
    first = cmd("frame")["frame"]
    deadline = time.monotonic() + 60
    while cmd("frame")["frame"] < first + n:
        if time.monotonic() > deadline:
            raise TimeoutError("guest frames did not advance")
        time.sleep(0.04)


stop_game()
env = dict(os.environ)
env["PSX_LOAD_SLOT"] = str(SLOT)
env["PSX_STEREO_SHED"] = "0"
for k in ("PSX_CYC_WATCH", "PSX_RECORD_FRAME", "PSX_OVERLAY_NATIVE_OFF", "PSX_IRQ_CHECK_BUDGET"):
    env.pop(k, None)
if INTERP:
    env["PSX_OVERLAY_NATIVE_OFF"] = "1"

log = OUT / "harness.log"
with log.open("w") as fh:
    proc = subprocess.Popen(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
         "-File", str(ROOT / "vr" / "run_vr.ps1"),
         "-Headless", "-Slot", str(SLOT), "-WeaponPoseDiagnostic", "-WristHud",
         "-WristHudReplace", "-BuildDirectory", "build-release", "-Seconds", "150"],
        cwd=str(ROOT), env=env, stdout=fh, stderr=subprocess.STDOUT)

t0 = time.monotonic()
while time.monotonic() - t0 < 240:
    try:
        cmd("frame", timeout=4.0)
        break
    except Exception:
        time.sleep(0.5)
else:
    proc.kill()
    raise SystemExit("no session")

st = cmd("savestate_status")
print("savestate:", json.dumps({k: st.get(k) for k in ("generation", "pending", "last_ok", "last_slot")}))
wait_frames(60)
try:
    from weapon_state import resume_if_paused
    resume_if_paused(cmd, wait_frames)
    print("resume_if_paused done")
except Exception as exc:
    print("resume skipped:", exc)

ov = cmd("overlay_loader_status")
print("overlay:", json.dumps({k: ov.get(k) for k in
                              ("registered", "dispatch_native", "dispatch_interp_fallback")}))
if ov.get("registered", 0) <= 0:
    proc.kill()
    raise SystemExit("route not registered (%r): refusing to compare the interpreter with itself"
                     % ov.get("registered"))

results = []
for name, axes in CASES:
    cmd("clear_input")
    cmd("openxr_input_override", clear=1)
    if axes:
        cmd("openxr_input_override", **axes)
    wait_frames(14)
    shot = (OUT / f"{name}.png").resolve().as_posix()
    ok = cmd("screenshot_file", path=shot)
    st = cmd("stereo_stats")
    pad = cmd("pad_status")
    # View-matrix numbers, the way check_movement.snapshot reads them: the level producer's
    # GTE ring entries carry RT/TR/H, and the player pointer leads to the camera struct.
    ring = cmd("gte_ring_dump", render=0, count=4096)
    ent = [e for e in ring.get("entries", []) if e.get("ra") == "0x8008BF84"]
    top = max(ent, key=lambda e: e["seq"]) if ent else None
    cam = None
    try:
        paddr = int.from_bytes(bytes.fromhex(cmd("read_ram", addr="0x8009d654", len=4)["hex"]), "little")
        if 0x80000000 <= paddr < 0x801ff000:
            cam = cmd("read_ram", addr=hex(paddr), len=256)["hex"]
    except Exception:
        pass
    results.append({"case": name, "axes": axes, "shot": ok,
                    "eye_hashes": st.get("eye_hashes"), "eye_cycles": st.get("eye_cycles"),
                    "pairs": st.get("pairs"), "shed": st.get("shed"),
                    "gte": ({k: top.get(k) for k in ("frame", "seq", "RT", "TR", "H")} if top else None),
                    "cam": cam,
                    "pad": pad.get("slot0", {}).get("sticks")})
    print(f"  {name:12s} axes={axes} gte={results[-1]['gte']} pad={results[-1]['pad']}")
    cmd("openxr_input_override", clear=1)
    cmd("clear_input")
    if proc.poll() is not None:
        print("harness exited early")
        break

(OUT / "sweep.json").write_text(json.dumps(results, indent=1) + "\n", encoding="utf-8")
print("wrote", OUT / "sweep.json")
stop_game()
