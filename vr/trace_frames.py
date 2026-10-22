"""Export the runtime's opt-in frame trace to CSV and a Perfetto/Chrome trace.

Run against a process launched with PSX_FRAME_TRACE=1. The runtime records one
row per stereo frame (guest-sim gap, per-eye begin/guest/end/restore, pair
total, XR wait/begin/acquire/wait-image/copy/release/end, shed/stale/failure
flags) with no per-frame allocation; this script only reads the ring over TCP
and writes files, so it never perturbs the frame path.

Outputs into <directory>:
    frame_trace.json         raw runtime export (records + counters)
    frame_trace.csv          one row per frame, stage columns in microseconds
    frame_trace_perfetto.json  Chrome trace-event format (optionally)
    summary.json             frame counts and stage percentiles
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "psxrecomp" / "tools"))
from debug_client import connect, send_cmd

# Mirrors PSX_FRAME_TRACE_* in psxrecomp/runtime/include/frame_trace.h. The JSON
# export carries the mask as an integer; the CSV carries the names.
FLAG_BITS = {
    1 << 0: "published",
    1 << 1: "stale",
    1 << 2: "shed",
    1 << 3: "failed",
    1 << 4: "xr_empty",
    1 << 5: "scene",
    1 << 6: "io",
    1 << 7: "no_telemetry",
    1 << 8: "baseline_reused",
    1 << 9: "xr_repeat",
}


def flag_names(mask):
    return {name for bit, name in FLAG_BITS.items() if mask & bit}


def command(name, **kwargs):
    with connect(timeout=60) as sock:
        result = send_cmd(sock, {"cmd": name, **kwargs})
    if not result.get("ok"):
        raise RuntimeError(result)
    return result


def save(path, name, obj):
    (path / name).write_text(json.dumps(obj, indent=2) + "\n", encoding="utf-8")


def percentile(values, pct):
    if not values:
        return None
    ordered = sorted(values)
    idx = min(len(ordered) - 1, int(round((pct / 100.0) * (len(ordered) - 1))))
    return ordered[idx]


def eye_total(record, side):
    stages = record["stages_us"]
    return (stages[f"eye_begin_{side}"] + stages[f"eye_guest_{side}"] +
            stages[f"eye_end_{side}"] + stages[f"eye_restore_{side}"])


def write_csv(path, records):
    stages = list(records[0]["stages_us"].keys()) if records else []
    head = ["seq", "open_ms", "guest_frame", "guest_cycle", "attempt", "pair_id",
            "pair_age_ms", "source", "eye_failed", "flags", "shed", "shed_ms",
            "prims", "tex_prims", "vram_leaks", "tex_bytes",
            "xr_display_delta_us", "xr_submit_fresh"] + stages + \
           ["failure", "note"]
    lines = [",".join(head)]
    for r in records:
        row = [r["seq"], r["open_ms"], r["guest_frame"], r["guest_cycle"],
               r["attempt"], r["pair_id"], r["pair_age_ms"], r["source"],
               r["eye_failed"], r["flags"], r["shed"], r["shed_ms"], r["prims"],
               r["tex_prims"], r["vram_leaks"], r["tex_bytes"],
               r.get("xr_display_delta_us", 0), r.get("xr_submit_fresh", 0)]
        row += [r["stages_us"][name] for name in stages]
        row += ['"%s"' % r["failure"], '"%s"' % r["note"]]
        lines.append(",".join(str(v) for v in row))
    (path / "frame_trace.csv").write_text("\n".join(lines) + "\n",
                                          encoding="utf-8")
    return head


def summarize(records, period_us=0):
    def series(fn):
        return [fn(r) for r in records]

    def stats(values):
        return {"p50_us": percentile(values, 50), "p95_us": percentile(values, 95),
                "p99_us": percentile(values, 99), "max_us": max(values) if values else None,
                "n": len(values)}

    stale = [r for r in records if "stale" in flag_names(r["flags"])]
    shed = [r for r in records if "shed" in flag_names(r["flags"])]
    failed = [r for r in records if "failed" in flag_names(r["flags"])]
    repeats = [r for r in records if "xr_repeat" in flag_names(r["flags"])]
    worst = sorted(records, key=lambda r: r["stages_us"]["pair_total"], reverse=True)[:8]
    # The runtime's own clock: how many granted display periods one presented
    # frame consumed. Above 1.0 means the compositor had to repeat a pair.
    deltas = [r.get("xr_display_delta_us", 0) for r in records
              if r.get("xr_display_delta_us")]
    cadence = {"n": len(deltas)}
    if deltas and period_us:
        ratios = [d / float(period_us) for d in deltas]
        cadence.update({
            "period_us": period_us,
            "periods_per_frame_p50": round(percentile(ratios, 50), 3),
            "periods_per_frame_p95": round(percentile(ratios, 95), 3),
            "periods_per_frame_mean": round(sum(ratios) / len(ratios), 3),
            "periods_served_by_a_repeat": round(sum(ratios) - len(ratios), 1),
        })
    return {
        "frames": len(records),
        "stale_frames": len(stale),
        "shed_frames": len(shed),
        "failed_frames": len(failed),
        "xr_repeat_frames": len(repeats),
        "cadence": cadence,
        "pose_age": stats(series(lambda r: r["stages_us"].get("pose_age", 0))),
        "max_pair_age_ms": max((r["pair_age_ms"] for r in records), default=0),
        "guest_sim": stats(series(lambda r: r["stages_us"]["guest_sim"])),
        "pair_total": stats(series(lambda r: r["stages_us"]["pair_total"])),
        "hud_capture": stats(series(lambda r: r["stages_us"].get("hud_capture", 0))),
        "hud_draw": stats(series(lambda r: r["stages_us"].get("hud_draw", 0))),
        "hud_readback": stats(series(lambda r: r["stages_us"].get("hud_readback", 0))),
        "eye_left_total": stats(series(lambda r: eye_total(r, "l"))),
        "eye_right_total": stats(series(lambda r: eye_total(r, "r"))),
        "xr_wait_frame": stats(series(lambda r: r["stages_us"]["xr_wait_frame"])),
        "xr_wait_image": stats(series(lambda r: r["stages_us"]["xr_wait_image"])),
        "worst_frames": [{"seq": r["seq"], "pair_total_us": r["stages_us"]["pair_total"],
                          "pair_age_ms": r["pair_age_ms"], "flags": r["flags"],
                          "note": r["note"]} for r in worst],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--max", type=int, default=240,
                        help="newest records to fetch (0 = all retained)")
    parser.add_argument("--perfetto", action="store_true",
                        help="also fetch the Chrome/Perfetto trace-event JSON")
    args = parser.parse_args()
    directory = args.directory.resolve()
    directory.mkdir(parents=True, exist_ok=True)

    result = command("frame_trace", format="json", max=args.max)
    trace = result["trace"]
    records = trace.get("records", [])
    save(directory, "frame_trace.json", result)
    write_csv(directory, records)
    summary = summarize(records, trace.get("target_period_us") or 0)
    summary["enabled"] = result["enabled"]
    summary["ring_count"] = result["count"]
    summary["ring_dropped"] = result["dropped"]
    summary["ring_missing"] = result["missing"]
    summary["game_revision"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    summary["framework_pin"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT / "psxrecomp", text=True).strip()
    if not result["enabled"]:
        summary["note"] = "PSX_FRAME_TRACE was off: relaunch with PSX_FRAME_TRACE=1"
    save(directory, "summary.json", summary)

    if args.perfetto:
        chrome = command("frame_trace", format="chrome", max=args.max)
        save(directory, "frame_trace_perfetto.json", chrome["trace"])

    print(json.dumps({k: summary[k] for k in
                      ("frames", "stale_frames", "shed_frames", "failed_frames",
                       "xr_repeat_frames", "max_pair_age_ms", "cadence", "pose_age",
                       "pair_total", "hud_capture", "hud_draw", "hud_readback",
                       "xr_wait_frame")}, indent=2))


if __name__ == "__main__":
    main()
