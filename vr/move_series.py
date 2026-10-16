"""View-matrix series across a scripted forward move, headless, both backends.

The look sweep sampled one matrix per pose and showed the two backends agree on the camera but
diverge once something moves, without saying where. This holds forward only and records the level
producer's GTE ring entry for every guest frame, keyed by the guest frame number, which is the
deterministic axis: RT/TR/H are the view matrices, and the entry carries the caller pc, so the
first frame the two backends disagree on also names the code that is running there.

Entries come from the ring's own frame filter rather than polling, so a sample cannot be missed or
duplicated and the two runs line up by construction.

Usage: move_series.py <tag> [interp] [slot] [frames]
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

ARGS = [a for a in sys.argv[2:]]
TAG = sys.argv[1]
INTERP = "interp" in ARGS
NUMS = [int(a) for a in ARGS if a.isdigit()]
SLOT = NUMS[0] if NUMS else 1
FRAMES = NUMS[1] if len(NUMS) > 1 else 150
OUT = ROOT / "analysis" / "vr-proof" / TAG
OUT.mkdir(parents=True, exist_ok=True)
LEVEL_PRODUCER = "0x8008BF84"


def stop_game():
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Stop-Process -Name Medal_of_Honor__Recompiled,vrserver,"
                    "vrmonitor,vrdashboard -Force -ErrorAction SilentlyContinue"],
                   capture_output=True)
    time.sleep(3)


def cmd(name, timeout=120.0, **kw):
    with connect(timeout=timeout) as sock:
        return send_cmd(sock, {"cmd": name, **kw})


def wait_frames(n, deadline_s=180):
    first = cmd("frame")["frame"]
    deadline = time.monotonic() + deadline_s
    while cmd("frame")["frame"] < first + n:
        if time.monotonic() > deadline:
            raise TimeoutError("guest frames did not advance")
        time.sleep(0.03)


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
         "-WristHudReplace", "-BuildDirectory", "build-release", "-Seconds", "300"],
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

cmd("clear_input")
cmd("openxr_input_override", clear=1)
# Overlays register lazily, so a window that starts too early measures the plain interpreter and
# would call the route innocent on no evidence. Wait for the same absolute frame the campaign's
# runs were live at, and report registration on both sides of the window so the run proves which
# backend it actually measured.
GATE = 220
while cmd("frame")["frame"] < GATE:
    time.sleep(0.05)
ov_gate = cmd("overlay_loader_status")
print("gate frame", cmd("frame")["frame"], "overlay:", json.dumps(
    {k: ov_gate.get(k) for k in ("registered", "dispatch_native", "dispatch_interp_fallback")}))
cmd("openxr_input_override", ly=1000, lx=0, rx=0, ry=0)
start_frame = cmd("frame")["frame"]
print("hold forward from frame", start_frame)

# Ask the ring for each frame explicitly instead of reading whatever is newest at poll time: the
# ring keeps entries per frame, so the sample cannot be missed, duplicated or attributed to the
# wrong frame, and the two backends line up by guest frame number.
#
# Every entry for the frame is kept, not just one producer's: the entries carry the caller (ra)
# and the full projection state (RT/TR/H, OFX/OFY, the projected vertices), so comparing the whole
# stream per frame finds the first frame the backends disagree on AND names the code responsible,
# which is the attribution the manifests then resolve to a shard or the exe.
KEEP = ("ra", "RT", "TR", "H", "OFX", "OFY", "V0", "V1", "V2", "SZ")
rows = {}
f = start_frame
target = start_frame + FRAMES
shape_printed = False
while f < target:
    wait_frames(1)
    f = cmd("frame")["frame"]
    r = cmd("gte_ring_dump", count=256, newest=1, frame=f, render=0)
    ents = sorted(r.get("entries", []), key=lambda e: e["seq"])
    if ents and not shape_printed:
        print("entry keys:", sorted(ents[0].keys()))
        print("sample entry:", json.dumps(ents[0]))
        print("entries in first sampled frame:", len(ents))
        shape_printed = True
    if ents:
        rows[f] = [{k: e.get(k) for k in KEEP} for e in ents]
end_frame = cmd("frame")["frame"]
ov2 = cmd("overlay_loader_status")
print("frames sampled", len(rows), "of", FRAMES, "window", start_frame, "..", end_frame)
print("overlay end:", json.dumps({k: ov2.get(k) for k in
                                  ("registered", "dispatch_native", "dispatch_interp_fallback")}))
cmd("openxr_input_override", clear=1)
cmd("clear_input")

(OUT / "series.json").write_text(json.dumps(
    {"tag": TAG, "interp": INTERP, "slot": SLOT, "start_frame": start_frame,
     "end_frame": end_frame, "entries": per_frame}, indent=1) + "\n", encoding="utf-8")
print("wrote", OUT / "series.json", len(per_frame), "frames",
      (min(per_frame) if per_frame else None), "..", (max(per_frame) if per_frame else None))
stop_game()
