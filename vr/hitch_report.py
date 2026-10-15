"""Attribute a stutter: which stage makes the hitch frames heavy.

A trace window is around 34 seconds of records, so a burst of slow frames does not
move the window's median and a per-window view will report "no slowdowns" for a
session where the player felt them. This reads the raw windows that
`vr/record_session.py` writes, dedupes by seq, ranks frames by pair time, and
compares the hitch frames against the rest stage by stage.

    python vr/hitch_report.py analysis/vr-proof/<name>
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
d = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "analysis" / "vr-proof" / "live"
THRESHOLD_US = int(sys.argv[2]) if len(sys.argv) > 2 else 12000

frames = {}
for path in sorted(d.glob("trace_raw_*.json")):
    for r in json.loads(path.read_text())["records"]:
        frames[r["seq"]] = r
if not frames:
    raise SystemExit(f"no trace_raw_*.json in {d}; run vr/record_session.py with raw windows on")
recs = [frames[k] for k in sorted(frames)]
print(f"{len(recs)} distinct frames, seq {recs[0]['seq']} to {recs[-1]['seq']}, from {d.name}")

KEYS = ["pair_total", "guest_sim", "eye_begin_l", "eye_guest_l", "eye_end_l", "eye_restore_l",
        "eye_begin_r", "eye_guest_r", "eye_end_r", "eye_restore_r", "hud_capture", "hud_draw",
        "hud_readback", "xr_end_frame", "xr_wait_frame", "pose_age"]


def stage(r, k):
    return r["stages_us"].get(k, 0)


def median(vals):
    vals = sorted(vals)
    return vals[len(vals) // 2] if vals else 0


med = {k: median([stage(r, k) for r in recs]) for k in KEYS}
print("\nmedian frame: " + "  ".join(f"{k}={med[k]}" for k in KEYS[:8]))
print("              " + "  ".join(f"{k}={med[k]}" for k in KEYS[8:]))

pairs = sorted(stage(r, "pair_total") for r in recs)
print(f"\npair_total: p50 {median(pairs)}  p95 {pairs[int(0.95 * (len(pairs) - 1))]}  max {pairs[-1]}")

print(f"\ntop 8 frames above {THRESHOLD_US} us and where their pair time sits:")
for r in sorted(recs, key=lambda r: -stage(r, "pair_total"))[:8]:
    total = stage(r, "pair_total")
    eyes = stage(r, "eye_guest_l") + stage(r, "eye_guest_r")
    ends = stage(r, "eye_end_l") + stage(r, "eye_end_r")
    begins = stage(r, "eye_begin_l") + stage(r, "eye_begin_r")
    restores = stage(r, "eye_restore_l") + stage(r, "eye_restore_r")
    print(f"  seq {r['seq']:>5d} pair {total:>6d} | eyes {eyes:>6d}"
          f" (+{eyes - med['eye_guest_l'] - med['eye_guest_r']:>+5d} vs median)"
          f"  end {ends:>5d}  begin {begins:>4d}  restore {restores:>4d}"
          f"  other {total - eyes - ends - begins - restores:>5d}"
          f" | hud {stage(r, 'hud_capture'):>5d}  xr_end {stage(r, 'xr_end_frame'):>5d}"
          f"  guest_sim {stage(r, 'guest_sim'):>6d}  note {str(r.get('note', ''))[:40]!r}")

slow = [r for r in recs if stage(r, "pair_total") > THRESHOLD_US]
fast = [r for r in recs if stage(r, "pair_total") <= THRESHOLD_US]
print(f"\nwhere the extra pair time goes, {len(slow)} slow frames of {len(recs)} "
      f"(medians per group):")
if slow and fast:
    for k in ("pair_total", "eye_guest_l", "eye_guest_r", "eye_end_l", "eye_end_r",
              "eye_begin_l", "eye_restore_l", "hud_capture", "hud_draw", "hud_readback",
              "xr_end_frame", "guest_sim", "pose_age"):
        s, f = median([stage(r, k) for r in slow]), median([stage(r, k) for r in fast])
        print(f"  {k:<14s} slow {s:>7d}  fast {f:>7d}  delta {s - f:>+7d}")

# Whether the extra eye time is geometry or uploads, which is the question the stage
# split cannot answer on its own. These are per-frame record counters, not stages.
print("\nper-frame counters that say what the eye replay was doing:")
for k in ("prims", "tex_prims", "tex_bytes", "vram_leaks"):
    s = median([r.get(k, 0) for r in slow])
    f = median([r.get(k, 0) for r in fast])
    print(f"  {k:<11s} slow {s:>9d}  fast {f:>9d}  delta {s - f:>+9d}")
