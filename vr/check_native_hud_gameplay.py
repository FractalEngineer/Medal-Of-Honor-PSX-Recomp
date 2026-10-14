"""Exercise native wrist art through slot-0 gameplay with synthetic controls.

Uses native formatted ammo strings, real compositor captures, and restored-pass
counters. This is unattended integration evidence, not headset comfort evidence.
"""
import argparse
from pathlib import Path
import time

import numpy as np
from PIL import Image

from capture_stereo import command, save
from check_movement import load, wait_frames
from headless_steamvr import Compositor


def state():
    def ram(address, length):
        return bytes.fromhex(command("read_ram", addr=hex(address), len=length)["hex"])
    player = int.from_bytes(ram(0x8009d654, 4), "little")
    assert 0x80000000 <= player <= 0x801ff000, hex(player)
    base = int.from_bytes(ram(player + 904, 4), "little")
    assert 0x80000000 <= base <= 0x801ff000, hex(base)
    def number(offset):
        text = ram(base + offset, 4).split(b"\0")[0].decode("ascii").strip()
        return int(text)
    return {"weapon": ram(base + 85, 1)[0], "clip": number(264), "reserve": number(268)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--steamvr-root", type=Path, required=True)
    args = parser.parse_args()
    out = args.directory.resolve() / "native-gameplay"
    out.mkdir(parents=True, exist_ok=False)
    compositor = Compositor(args.steamvr_root)
    result = {"passed": False, "synthetic_controls": True, "slot": 0}
    images = {}
    def capture(name, menu=False):
        time.sleep(1.1)
        record = compositor.capture(out / name, args.pid, require_projection=not menu)
        stats = command("openxr_stats")
        assert stats["submitted_layer"] == (2 if menu else 1), stats
        assert stats["submitted_surfaces"] == (0 if menu else 2), stats
        save(out, name + ".json", {"capture": record, "openxr": stats})
        images[name] = np.asarray(Image.open(out / name / "compositor.png").convert("RGB"))
    def press(**fields):
        command("openxr_input_override", **fields)
        wait_frames(4)
        command("openxr_input_override", clear=1)
        wait_frames(90)
    def changed(a, b):
        # Static synthetic grips locate the right wrist in this ROI in each eye.
        for eye in range(2):
            x = eye * (images[a].shape[1] // 2)
            before = images[a][650:790, x+650:x+850].astype(int)
            after = images[b][650:790, x+650:x+850].astype(int)
            assert np.count_nonzero(abs(before-after).max(axis=2)>30) > 20, (a,b,eye)
    try:
        load(0)
        for hand, x in (("left", -180), ("right", 180)):
            command("openxr_hands_override", hand=hand, pose="grip", active=1,
                    focused=1, flags=15, px_mm=x, py_mm=-180, pz_mm=-450,
                    qz=707107 if hand=="left" else -707107, qw=707107)
        wait_frames(6)
        initial = state()
        assert initial["clip"] > 0
        capture("loaded")
        press(right_trigger=1000)
        fired = state()
        assert fired["weapon"] == initial["weapon"] and fired["clip"] < initial["clip"], (initial, fired)
        capture("fired"); changed("loaded", "fired")
        press(left_buttons=1)
        reloaded = state()
        assert reloaded["clip"] > fired["clip"] and reloaded["reserve"] < fired["reserve"], (fired, reloaded)
        capture("reloaded"); changed("fired", "reloaded")
        press(right_buttons=2)
        switched = state()
        assert switched["weapon"] != reloaded["weapon"], (reloaded, switched)
        capture("switched"); changed("reloaded", "switched")
        press(left_buttons=4); capture("paused", menu=True)
        press(left_buttons=4); capture("resumed")
        load(0)
        restored = state()
        assert restored == initial, (initial, restored)
        capture("save-reloaded")
        checks = command("render_pass_stats")
        assert checks["verify_checks"] > 0, checks
        for key in ("verify_mismatch", "vram_leaks", "nesting_repairs", "watchdog"):
            assert not checks.get(key, 0), checks
        save(out, "render_pass_stats.json", checks)
        result.update(passed=True, initial=initial, fired=fired, reloaded=reloaded,
                      switched=switched, save_reloaded=restored)
    except Exception as exc:
        result["error"] = str(exc)
        raise
    finally:
        try:
            command("clear_input")
            command("openxr_input_override", clear=1)
            command("openxr_hands_override", clear=1)
        finally:
            compositor.close()
            save(out, "result.json", result)
    print("Native wrist HUD fire/reload/switch/pause/save-load checks passed.")


if __name__ == "__main__":
    main()
