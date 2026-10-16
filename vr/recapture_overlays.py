"""Recapture the overlays this level actually loads, while roaming it.

The loader records a capture when the game DMA-loads an overlay, but the JSON is only written by an
explicit dump or a clean window close, so a killed run leaves the index stale: that is why the
loader reported regions_checked with file_found 0 and nothing registered even though 67 shards were
present. This roams the seeded level with the route off (shards moved aside), dumping every block,
and reports how many capture entries and regions the loader actually saw.

Usage: recapture_overlays.py [slot] [blocks]
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

SLOT = sys.argv[1] if len(sys.argv) > 1 else "1"
BLOCKS = int(sys.argv[2]) if len(sys.argv) > 2 else 14
OUT = ROOT / "analysis" / "vr-proof" / "recapture"
OUT.mkdir(parents=True, exist_ok=True)


def stop_game():
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Stop-Process -Name Medal_of_Honor__Recompiled -Force -ErrorAction SilentlyContinue"],
                   capture_output=True)
    time.sleep(3)


def cmd(name, timeout=90.0, **kw):
    with connect(timeout=timeout) as sock:
        return send_cmd(sock, {"cmd": name, **kw})


def wait_frames(n, deadline_s=180):
    first = cmd("frame")["frame"]
    deadline = time.monotonic() + deadline_s
    while cmd("frame")["frame"] < first + n:
        if time.monotonic() > deadline:
            raise TimeoutError("guest frames did not advance")
        time.sleep(0.03)


def brief(name, **kw):
    d = cmd(name, **kw)
    return {k: v for k, v in d.items() if not isinstance(v, (list, dict))}


stop_game()
env = dict(os.environ)
if SLOT != "none":
    env["PSX_LOAD_SLOT"] = SLOT
env["PSX_STEREO_SHED"] = "0"
launch = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
          "-File", str(ROOT / "vr" / "run_vr.ps1"), "-Headless"]
if SLOT != "none":
    launch += ["-Slot", SLOT]
launch += ["-BuildDirectory", "build-release", "-Seconds", "600"]
log = OUT / "harness.log"
with log.open("w") as fh:
    proc = subprocess.Popen(launch, cwd=str(ROOT), env=env, stdout=fh, stderr=subprocess.STDOUT)

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

print("savestate:", json.dumps(brief("savestate_status")))
wait_frames(60)
try:
    from weapon_state import resume_if_paused
    resume_if_paused(cmd, wait_frames)
    print("resumed")
except Exception as exc:
    print("resume:", exc)

rows = []
for i in range(BLOCKS):
    # Forward with a slow alternating turn: the level streams overlays as it is roamed, and the
    # turn keeps the walk from stalling against one wall.
    rx = (0, 400, 800, 400, 0, -400, -800, -400)[i % 8]
    cmd("openxr_input_override", ly=1000, lx=0, rx=rx, ry=0)
    wait_frames(40)
    d = cmd("overlay_capture_dump")
    st = brief("overlay_loader_status")
    row = {"block": i, "frame": cmd("frame")["frame"], "rx": rx,
           "capture_entries": d.get("capture_entries"),
           "regions_checked": st.get("regions_checked"), "checked": st.get("checked"),
           "registered": st.get("registered"), "file_found": st.get("file_found"),
           "loads": st.get("loads"), "last_crc": st.get("last_crc")}
    rows.append(row)
    print("block %2d frame %5d rx %5d entries %3s regions %s checked %s found %s" %
          (i, row["frame"], rx, row["capture_entries"], row["regions_checked"],
           row["checked"], row["file_found"]))
    if proc.poll() is not None:
        print("harness exited early")
        break

cmd("openxr_input_override", clear=1)
cmd("clear_input")
d = cmd("overlay_capture_dump")
print("final capture entries:", d.get("capture_entries"))
(OUT / "recapture.json").write_text(json.dumps(rows, indent=1) + "\n", encoding="utf-8")
stop_game()
