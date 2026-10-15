"""Record a whole VR session, from before the game starts to after it exits.

Run this first, then launch the game: it waits for the debug port, then samples once
a second and flushes each row, so a crash, a hung frame or a dead socket shows up in
the record instead of truncating it. Nothing here drives input, so a session with a
player in the headset is unaffected.

    python vr/record_session.py analysis/vr-proof/<name>

Outputs into that directory:

    live.csv                one row per second: layer, wrist surface submissions and
                            uploads, XR submissions and losses, pairs, shedding,
                            checkpoint saves and reuses, and rates per second
    trace.jsonl             every VR_SAMPLE_S seconds: stage percentiles for the pair,
                            the eyes, the HUD capture and the XR path, plus the three
                            worst frames of the window whole
    trace_raw_<t>s.json     the raw records of each trace window, so a burst that a
                            window median would hide can still be attributed later
    record.log              what the recorder did, including stalls

Environment: VR_RECORD_S (max seconds, 1800), VR_WAIT_S (how long to wait for the
port, 300), VR_SAMPLE_S (sample interval, 1), VR_TRACE_EVERY_S (10), VR_STALL_LIMIT
(samples with no response before stopping, 25), VR_RAW_TRACE (write windows, 1),
VR_APPEND (append to an existing csv, 0).

Two notes learned the hard way. Launching the game first loses the boot and the first
findings, so start this first. And the frame_trace response nests the export under a
"trace" key and only returns the oldest/default window unless max is passed, which is
why the snapshot asks for 1024 explicitly.
"""
import csv
import json
import os
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "psxrecomp" / "tools"))
from debug_client import connect, send_cmd  # noqa: E402

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "analysis" / "vr-proof" / "live"
MAX_S = float(os.environ.get("VR_RECORD_S", "1800"))
WAIT_S = float(os.environ.get("VR_WAIT_S", "300"))
INTERVAL = float(os.environ.get("VR_SAMPLE_S", "1"))
TRACE_EVERY = float(os.environ.get("VR_TRACE_EVERY_S", "10"))
STALL_LIMIT = int(os.environ.get("VR_STALL_LIMIT", "25"))

OUT.mkdir(parents=True, exist_ok=True)
logf = (OUT / "record.log").open("a", buffering=1, encoding="utf-8")


def say(msg):
    line = f"{time.strftime('%H:%M:%S')} {msg}"
    print(line, flush=True)
    logf.write(line + "\n")


def cmd(name, timeout=4.0, **kw):
    with connect(timeout=timeout) as sock:
        return send_cmd(sock, {"cmd": name, **kw})


def as_int(d, key):
    try:
        return int(d.get(key, 0))
    except (TypeError, ValueError):
        return 0


def as_float(d, key):
    try:
        return float(d.get(key, 0.0))
    except (TypeError, ValueError):
        return 0.0


COLS = ["clock", "elapsed_s", "alive", "layer", "surfaces", "surface_uploads",
        "surface_failures", "native_submitted", "submitted", "empty", "failures", "losses",
        "tracking", "pairs", "shed", "stale", "failed", "shedding", "pair_ms", "ckpt_saves",
        "ckpt_reuses", "ckpt_bytes", "pairs_per_s", "saves_per_s", "note"]
csv_path = OUT / "live.csv"
APPEND = os.environ.get("VR_APPEND", "0") == "1" and csv_path.exists()
if APPEND and csv_path.read_text(encoding="utf-8").splitlines()[0].split(",")[0] != "clock":
    say("existing csv predates the clock column; starting a new file")
    csv_path = OUT / f"live-{time.strftime('%H%M%S')}.csv"
    APPEND = False
csvf = csv_path.open("a" if APPEND else "w", newline="", encoding="utf-8", buffering=1)
writer = csv.DictWriter(csvf, fieldnames=COLS)
if not APPEND:
    writer.writeheader()

say(f"recording to {OUT} (interval {INTERVAL}s, trace every {TRACE_EVERY}s, max {MAX_S}s)")
say("waiting for the debug port")

t_wait = time.time()
while time.time() - t_wait < WAIT_S:
    try:
        if cmd("openxr_stats").get("running") is not None:
            break
    except Exception:
        pass
    time.sleep(0.5)
else:
    say("no session appeared within the wait window; stopping")
    raise SystemExit(1)

say("session up")
t0 = time.time()
prev = None
stalls = 0
last_trace = 0.0
worst = {"pair_ms": 0.0, "shed": 0}
panels_seen = game_layer_seen = capture_seen = False

while time.time() - t0 < MAX_S:
    elapsed = round(time.time() - t0, 1)
    note = ""
    xr = st = None
    try:
        xr = cmd("openxr_stats")
    except Exception as exc:
        note = f"openxr_stats:{type(exc).__name__}"
    try:
        st = cmd("stereo_stats")
    except Exception as exc:
        note = (note + " " if note else "") + f"stereo_stats:{type(exc).__name__}"

    if xr is None and st is None:
        stalls += 1
        writer.writerow({**{c: "" for c in COLS}, "elapsed_s": elapsed, "alive": 0,
                         "note": note or "no response"})
        if stalls <= 3 or stalls % 5 == 0:
            say(f"{elapsed:7.1f}s STALL {stalls} ({note})")
        if stalls >= STALL_LIMIT:
            say(f"debug surface dead for {stalls} samples; stopping the recording")
            break
        time.sleep(INTERVAL)
        continue

    stalls = 0
    xr = xr or {}
    st = st or {}
    pairs, shed = as_int(st, "pairs"), as_int(st, "shed")
    saves, reuses = as_int(st, "checkpoint_saves"), as_int(st, "checkpoint_reuses")
    row = {
        "clock": time.strftime("%H:%M:%S"), "elapsed_s": elapsed, "alive": 1,
        "layer": as_int(xr, "submitted_layer"), "surfaces": as_int(xr, "submitted_surfaces"),
        "surface_uploads": as_int(xr, "surface_uploads"),
        "surface_failures": as_int(xr, "surface_failures"),
        "native_submitted": as_int(xr, "native_submitted"),
        "submitted": as_int(xr, "submitted"), "empty": as_int(xr, "empty"),
        "failures": as_int(xr, "failures"), "losses": as_int(xr, "losses"),
        "tracking": as_int(xr, "tracking"),
        "pairs": pairs, "shed": shed, "stale": as_int(st, "stale_frames"),
        "failed": as_int(st, "failed"), "shedding": as_int(st, "shedding"),
        "pair_ms": round(as_float(st, "avg_pair_ms"), 2),
        "ckpt_saves": saves, "ckpt_reuses": reuses,
        "ckpt_bytes": as_int(st, "checkpoint_ram_bytes"),
        "note": note,
    }
    if prev:
        dt = elapsed - prev["elapsed_s"]
        if dt > 0:
            row["pairs_per_s"] = round((pairs - prev["pairs"]) / dt, 1)
            row["saves_per_s"] = round((saves - prev["ckpt_saves"]) / dt, 1)
    writer.writerow(row)
    prev = row

    if as_int(xr, "submitted_surfaces") >= 2:
        panels_seen = True
    if as_int(xr, "submitted_layer") == 1:
        game_layer_seen = True
    if row.get("saves_per_s", 0):
        capture_seen = True
    worst["pair_ms"] = max(worst["pair_ms"], row["pair_ms"])
    worst["shed"] = max(worst["shed"], shed)

    if elapsed - last_trace >= TRACE_EVERY:
        last_trace = elapsed
        try:
            raw = cmd("frame_trace", timeout=25.0, max=1024)
            blob = raw.get("trace")
            data = json.loads(blob) if isinstance(blob, str) else (blob or {})
            records = data.get("records") or []

            def pct(stage, p):
                vals = sorted(r["stages_us"][stage] for r in records
                              if r.get("stages_us", {}).get(stage))
                if not vals:
                    return None
                return vals[min(len(vals) - 1, int(round((p / 100.0) * (len(vals) - 1))))]

            snap = {
                "clock": time.strftime("%H:%M:%S"), "elapsed_s": elapsed,
                "layer": row["layer"], "surfaces": row["surfaces"],
                "runtime": data.get("runtime"), "target_period_us": data.get("target_period_us"),
                "count": data.get("count"), "dropped": data.get("dropped"),
                "records": len(records),
                "stages": {s: {"p50_us": pct(s, 50), "p95_us": pct(s, 95)}
                           for s in ("hud_capture", "hud_draw", "hud_readback", "pair_total",
                                     "guest_sim", "xr_wait_frame", "pose_age",
                                     "eye_begin_l", "eye_guest_l", "eye_end_l", "eye_restore_l",
                                     "eye_begin_r", "eye_guest_r", "eye_end_r", "eye_restore_r")},
                # A burst inside a window barely moves its median, so keep the frames
                # that stutter whole rather than only their percentiles.
                "worst_frames": [
                    {"seq": r["seq"], "pair_total_us": r["stages_us"]["pair_total"],
                     "pair_age_ms": r["pair_age_ms"], "flags": r["flags"],
                     "guest_sim_us": r["stages_us"]["guest_sim"],
                     "stages": {k: v for k, v in r["stages_us"].items() if v},
                     "note": str(r.get("note", ""))[:80],
                     "failure": str(r.get("failure", ""))[:40]}
                    for r in sorted(records, key=lambda r: -r["stages_us"]["pair_total"])[:3]],
            }
            if os.environ.get("VR_RAW_TRACE", "1") == "1":
                (OUT / f"trace_raw_{int(elapsed):04d}s.json").write_text(
                    json.dumps({"elapsed_s": elapsed, "records": records}), encoding="utf-8")
            (OUT / "trace.jsonl").open("a", encoding="utf-8").write(json.dumps(snap) + "\n")
        except Exception as exc:
            say(f"trace snapshot failed: {type(exc).__name__}")

    time.sleep(INTERVAL)

csvf.close()
say(f"stopped: panels_seen={panels_seen} gameplay_layer_seen={game_layer_seen} "
    f"capture_active={capture_seen} worst_pair_ms={worst['pair_ms']} sheds_total={worst['shed']}")
say(f"files: {csv_path} {OUT / 'trace.jsonl'}")
