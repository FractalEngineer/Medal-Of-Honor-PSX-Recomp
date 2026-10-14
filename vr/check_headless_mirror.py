"""Require advancing XR gameplay, restore verification and real compositor PNGs.

Foundation for wrist-HUD acceptance; this does not claim an unimplemented HUD
has passed. Synthetic hands are explicitly marked and always cleared.
"""
import argparse
import json
from pathlib import Path
import time

from capture_stereo import command, save
from headless_steamvr import Compositor


def check_progress(before, after):
    if not after.get("running") or not after.get("tracking"):
        raise RuntimeError("OpenXR is not running and tracking")
    if before.get("state") != 5 or after.get("state") != 5:
        raise RuntimeError("OpenXR application must be focused; dashboard/overlays may obscure captures")
    if after.get("submitted", 0) <= before.get("submitted", 0):
        raise RuntimeError("OpenXR submissions did not advance")
    if after.get("submitted_layer") != 1 or after.get("submitted_source") != 1:
        raise RuntimeError("Expected genuine stereo gameplay projection, not menu/native surface")
    if after.get("failures", 0) != before.get("failures", 0):
        raise RuntimeError("OpenXR failures increased during capture")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--steamvr-root", type=Path, required=True)
    args = parser.parse_args()
    output = args.directory.resolve() / "headless-mirror"
    output.mkdir(parents=True, exist_ok=False)
    compositor = None
    result = {"passed": False, "wrist_hud_verified": False,
              "scope": "XR gameplay/compositor and synthetic grip harness"}
    overrides_touched = False
    try:
        baseline = command("openxr_stats")
        save(output, "before.json", baseline)
        if baseline.get("runtime") != "SteamVR/OpenXR":
            raise RuntimeError("This capture backend requires SteamVR/OpenXR")
        compositor = Compositor(args.steamvr_root)
        scenarios = [
            ("both-hands", 1, 1, 1),
            ("left-lost", 0, 1, 1),
            ("right-lost", 1, 0, 1),
            ("unfocused", 1, 1, 0),
            ("recovered", 1, 1, 1),
        ]
        for name, left, right, focused in scenarios:
            overrides_touched = True
            for hand, active, x in (("left", left, -180), ("right", right, 180)):
                command("openxr_hands_override", hand=hand, pose="grip",
                        active=active, focused=focused, flags=15,
                        px_mm=x, py_mm=-180, pz_mm=-450)
            snapshot = command("openxr_hands")
            if not snapshot.get("synthetic") or snapshot.get("focused") != focused:
                raise RuntimeError(f"Synthetic hand scenario did not apply: {name}")
            # Unfocus clears both poses. Recovery must rebuild both from scratch.
            for side, active in enumerate((left, right)):
                if snapshot["pose"][side][0]["active"] != (active if focused else 0):
                    raise RuntimeError(f"Unexpected grip activity: {name}")
            save(output, f"{name}-hands.json", snapshot)
            before = command("openxr_stats")
            time.sleep(1.1)  # dumps have second-resolution filenames
            capture = compositor.capture(output / name, args.pid)
            after = command("openxr_stats")
            check_progress(before, after)
            save(output, f"{name}.json", {"capture": capture, "before": before, "after": after})
        restore = command("render_pass_stats")
        save(output, "render_pass_stats.json", restore)
        # Require actual verification, not a zero mismatch count while disabled.
        if restore.get("verify_checks", 0) <= 0:
            raise RuntimeError("Render-pass verification must be enabled")
        for counter in ("verify_mismatch", "vram_leaks", "nesting_repairs", "watchdog"):
            if restore.get(counter, 0):
                raise RuntimeError(f"Render-pass failure: {counter}={restore[counter]}")
        result["passed"] = True
    except Exception as exc:
        result["error"] = str(exc)
        raise
    finally:
        try:
            if overrides_touched:
                save(output, "hands_cleared.json", command("openxr_hands_override", clear=1))
        except Exception as exc:
            result["passed"] = False
            result["cleanup_error"] = str(exc)
            raise
        finally:
            if compositor:
                compositor.close()
            save(output, "result.json", result)
    print(f"Headless mirror harness passed: {output}; wrist HUD acceptance remains pending")


if __name__ == "__main__":
    main()
