"""Head bob, headless: trace full circles with the HEAD POSITION and capture every step.

Two phases, both pure translation (no rotation), which is what the pose override offers:

  front-circle     right -> down -> left -> up, a circle in the frontal (X/Y) plane
  sagittal-circle  forward -> down -> back -> up, a circle in the sagittal (Z/Y) plane

Each step sets the head offset, waits a few guest frames, and records a screenshot, the guest
view matrices and the pose the renderer actually used, so the motion can be verified rather than
assumed. Run once with the shards present and once with them moved aside to compare backends.

Usage: headbob.py <tag> [interp] [radius_mm]
"""
import json
import math
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(r"C:\Users\titan\Desktop\Github_Projects\Mine\Medal-Of-Honor-PSX-Recomp")
sys.path.insert(0, str(ROOT / "psxrecomp" / "tools"))
from debug_client import connect, send_cmd  # noqa: E402

TAG = sys.argv[1]
INTERP = "interp" in sys.argv[2:]
RADIUS = float(next((a for a in sys.argv[2:] if a.isdigit()), 60))
STEPS = 16
OUT = ROOT / "analysis" / "vr-proof" / TAG
OUT.mkdir(parents=True, exist_ok=True)


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
        time.sleep(0.03)


def offsets(phase, step):
    """Translation offsets in mm for one step of a phase's circle."""
    a = 2.0 * math.pi * step / STEPS
    x = RADIUS * math.cos(a)
    y = -RADIUS * math.sin(a)
    if phase == "front-circle":
        return x, y, 0.0
    return 0.0, y, -RADIUS * math.cos(a)   # -Z is forward


stop_game()
env = dict(os.environ)
env["PSX_LOAD_SLOT"] = "1"
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
         "-Headless", "-Slot", "1", "-WeaponPoseDiagnostic", "-WristHud",
         "-WristHudReplace", "-BuildDirectory", "build-release", "-Seconds", "240"],
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
    print("resumed")
except Exception as exc:
    print("resume skipped:", exc)
ov = cmd("overlay_loader_status")
print("overlay:", ov.get("registered"), "shards, native", ov.get("dispatch_native"))
if ov.get("registered", 0) <= 0:
    proc.kill()
    raise SystemExit("route not registered (%r): refusing to compare the interpreter with itself"
                     % ov.get("registered"))

rows = []
for phase in ("front-circle", "sagittal-circle"):
    for step in range(STEPS):
        x, y, z = offsets(phase, step)
        r = cmd("openxr_pose_override", x_mm=int(round(x)), y_mm=int(round(y)), z_mm=int(round(z)))
        wait_frames(3)
        shot = (OUT / f"{phase}-{step:02d}.png").resolve().as_posix()
        cmd("screenshot_file", path=shot)
        xr = cmd("openxr_stats")
        st = cmd("stereo_stats")
        ring = cmd("gte_ring_dump", render=0, count=2048)
        ent = [e for e in ring.get("entries", []) if e.get("ra") == "0x8008BF84"]
        top = max(ent, key=lambda e: e["seq"]) if ent else {}
        rows.append({"phase": phase, "step": step,
                     "set_mm": [int(round(x)), int(round(y)), int(round(z))],
                     "ack": r.get("x_mm"), "frame": cmd("frame")["frame"],
                     "pose": xr.get("pose") if isinstance(xr.get("pose"), list) else xr.get("pose"),
                     "RT": top.get("RT"), "TR": top.get("TR"), "H": top.get("H"),
                     "eye_hashes": st.get("eye_hashes")})
        # Pose age is the metric the compositor reprojects from, so it is the one that speaks to
        # "shifts and stretches when I move": a stale pose means the compositor warps the frame.
        try:
            tr = cmd("frame_trace", max=4)
            rows[-1]["trace"] = tr.get("trace")
        except Exception as exc:
            rows[-1]["trace_err"] = str(exc)
        print(f"  {phase:16s} {step:02d} set={rows[-1]['set_mm']} frame={rows[-1]['frame']} H={top.get('H')}")
        if proc.poll() is not None:
            print("harness exited early")
            break

cmd("openxr_pose_override", clear=1)
(OUT / "headbob.json").write_text(json.dumps(rows, indent=1) + "\n", encoding="utf-8")
print("wrote", OUT / "headbob.json")
stop_game()
