"""Capture real eye pairs across a deterministic millimetre head-motion sweep.

Run against an already owned headless session. Does not save or move the
player; --slot optionally loads an isolated diagnostic state. The ceiling/wall crop excludes the animated weapon and native HUD.
Unique crops measure pose plateaus, not headset motion-to-photon latency.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import statistics

from PIL import Image, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "psxrecomp" / "tools"))
from debug_client import connect, send_cmd


def command(name, **kwargs):
    with connect(timeout=30) as sock:
        result = send_cmd(sock, {"cmd": name, **kwargs})
    if not result.get("ok"):
        raise RuntimeError(result)
    return result


def capture(directory):
    directory.mkdir()
    first = command("stereo_stats")["pairs"]
    deadline = time.monotonic() + 20
    while command("stereo_stats")["pairs"] < first + 3:
        if time.monotonic() > deadline:
            raise TimeoutError("No fresh stereo pair")
        time.sleep(.02)
    command("stereo_dump", path=directory.as_posix(), count=1)
    while True:
        records = command("stereo_stats").get("captures", [])
        for record in reversed(records):
            stem = f"p{record['pair_id']:06d}"
            files = [directory / f"{stem}_{eye}.png" for eye in ("left", "right", "sbs")]
            if all(p.exists() for p in files):
                for p in files:
                    with Image.open(p) as im:
                        im.load()
                return record, files[:2]
        if time.monotonic() > deadline:
            raise TimeoutError("No complete eye dump")
        time.sleep(.03)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--steps", type=int, default=21)
    parser.add_argument("--axis", choices=("x", "y", "z"), default="x")
    parser.add_argument("--step-mm", type=int, default=1)
    parser.add_argument("--pitch", type=int, default=0)
    parser.add_argument("--pitch-step-mdeg", type=int, default=0)
    parser.add_argument("--slot", type=int, help="Load a slot from the running session's isolated save directory")
    args = parser.parse_args()
    directory = args.directory.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    rows, previous = [], [None, None]
    try:
        if args.slot is not None:
            generation = command("savestate_status")["generation"]
            command("savestate", op="load", slot=args.slot)
            deadline = time.monotonic() + 20
            while True:
                state = command("savestate_status")
                if state["generation"] > generation and not state["pending"]:
                    if not state["last_ok"]:
                        raise RuntimeError(state)
                    break
                if time.monotonic() > deadline:
                    raise TimeoutError("Slot did not load")
                time.sleep(.03)
        for i in range(args.steps):
            pose = {"x_mm": 0, "y_mm": 0, "z_mm": 0, "pitch_deg": args.pitch}
            if args.pitch_step_mdeg:
                pose["pitch_mdeg"] = i * args.pitch_step_mdeg
            pose[f"{args.axis}_mm"] = i * args.step_mm
            command("openxr_pose_override", **pose)
            pair, files = capture(directory / f"step-{i:03d}")
            row = {"pose": pose, "pair": pair, "views": command("openxr_views"), "eyes": []}
            for eye, path in enumerate(files):
                with Image.open(path) as im:
                    im = im.convert("RGB")
                    w, h = im.size
                    crop = im.crop((int(w*.05), int(h*.26), int(w*.95), int(h*.40)))
                digest = hashlib.sha256(crop.tobytes()).hexdigest()
                delta = None if previous[eye] is None else sum(ImageStat.Stat(
                    ImageChops.difference(previous[eye], crop)).mean) / 3
                previous[eye] = crop
                row["eyes"].append({"path": str(path), "world_sha256": digest, "mean_delta": delta})
            rows.append(row)
        result = {"rows": rows, "unique_world_crops": [len({r["eyes"][e]["world_sha256"] for r in rows}) for e in range(2)],
                  "render_pass": command("render_pass_stats"), "stereo": command("stereo_stats"),
                  "counters": command("mod_counters"), "pgxp": command("pgxp")}
        result["motion"] = []
        for eye in range(2):
            deltas = [r["eyes"][eye]["mean_delta"] for r in rows[1:]]
            mean = statistics.mean(deltas) if deltas else 0
            result["motion"].append({"moving_steps_above_0_01": sum(d > .01 for d in deltas),
                "steps": len(deltas), "mean_delta": mean,
                "delta_cv": statistics.pstdev(deltas) / mean if mean else 0})
        (directory / "receipt.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps({"steps": len(rows), "unique_world_crops": result["unique_world_crops"],
                          "verify_checks": result["render_pass"]["verify_checks"],
                          "verify_mismatch": result["render_pass"]["verify_mismatch"],
                          "motion": result["motion"]}))
    finally:
        command("openxr_pose_override", clear=1)


if __name__ == "__main__":
    main()
