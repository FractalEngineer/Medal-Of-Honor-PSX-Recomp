"""Read-only movie pacing sample. Isolate active MDEC/native-surface intervals."""
import argparse
import time
from pathlib import Path
from capture_stereo import command, save


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--seconds", type=float, default=20)
    args = parser.parse_args()
    args.directory.mkdir(parents=True, exist_ok=True)
    rows = []
    deadline = time.perf_counter() + args.seconds
    while time.perf_counter() < deadline:
        before = time.perf_counter()
        frame = command("frame")
        after = time.perf_counter()
        rows.append({"time": (before + after) * .5, "frame_query_ms": (after-before)*1000,
                     "frame": frame, "xr": command("openxr_stats"),
                     "fmv": command("fmv_state"), "gpu": command("gpu_state"),
                     "video": command("video_info"), "turbo": command("turbo_state")})
        time.sleep(.5)
    save(args.directory, "samples.json", rows)
    seconds = frames = decoded = intervals = 0
    swap_intervals = set()
    for a, b in zip(rows, rows[1:]):
        delta = b["fmv"]["mdec_decode_count"] - a["fmv"]["mdec_decode_count"]
        if not (delta > 0 and a["xr"]["submitted_source"] == b["xr"]["submitted_source"] == 2
                and not a["turbo"]["enabled"] and not b["turbo"]["enabled"]
                and (a["gpu"]["width"], a["gpu"]["height"]) == (b["gpu"]["width"], b["gpu"]["height"])):
            continue
        seconds += b["time"] - a["time"]
        frames += b["frame"]["frame"] - a["frame"]["frame"]
        decoded += delta
        intervals += 1
        swap_intervals.add(b["video"]["gl_swap_interval"])
    summary = {"intervals": intervals, "seconds": seconds, "guest_frames": frames,
               "decoded_frames": decoded, "actual_gl_swap_intervals": sorted(swap_intervals),
               "guest_hz": frames/seconds if seconds else None,
               "decoded_hz": decoded/seconds if seconds else None,
               "note": "Adjacent queries, native MDEC intervals only; not display motion-to-photon proof."}
    save(args.directory, "summary.json", summary)
    print(summary, flush=True)


if __name__ == "__main__":
    main()
