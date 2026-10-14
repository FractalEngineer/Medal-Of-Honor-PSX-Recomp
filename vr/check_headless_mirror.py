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


def panel_pixels(path, expected):
    """Identify diagnostic borders in each composited eye, not the source atlas."""
    import numpy as np
    from PIL import Image
    rgb = np.asarray(Image.open(path).convert("RGB")).astype(int)
    masks = ((rgb[:,:,0]<60)&(rgb[:,:,1]>150)&(rgb[:,:,2]>150),
             (rgb[:,:,0]>160)&(rgb[:,:,1]<65)&(rgb[:,:,2]>140))
    measurements = []
    for side, (mask, visible) in enumerate(zip(masks, expected)):
        eyes=[]
        for eye in range(2):
            half=mask[:,eye*(rgb.shape[1]//2):(eye+1)*(rgb.shape[1]//2)]
            y,x=np.nonzero(half)
            if (len(x)>=80) != bool(visible):
                raise RuntimeError(f"Diagnostic panel {side}, eye {eye}: expected visible={visible}, pixels={len(x)}")
            eyes.append({"pixels":len(x), "centroid": [float(x.mean()),float(y.mean())] if len(x) else None,
                         "bounds":[int(x.min()),int(y.min()),int(x.max()),int(y.max())] if len(x) else None})
        measurements.append(eyes)
    return measurements


def native_hud_pixels(path, expected, replace):
    """Seeded slot-0 sky-scene assertions, not a general HUD classifier.

    Original health ring is green, ammo numerals are neutral bright pixels.
    ROIs are verification only; production extraction uses native producers.
    """
    import numpy as np
    from PIL import Image
    rgb=np.asarray(Image.open(path).convert("RGB")).astype(int)
    half=rgb.shape[1]//2;results=[]
    for eye in range(2):
        image=rgb[:,eye*half:(eye+1)*half]
        green=(image[:,:,1]>20)&(image[:,:,1]>image[:,:,0]*1.3)&(image[:,:,1]>image[:,:,2]*1.3)
        grey=(image.min(axis=2)>100)&((image.max(axis=2)-image.min(axis=2))<20)
        counts=[int(green[500:1050,80:600].sum()),int(grey[600:950,650:half].sum()),
                int(green[80:350,:230].sum()),int(grey[50:200,760:half].sum())]
        wanted=[bool(expected[0]),bool(expected[1]),not replace or not expected[0],not replace or not expected[1]]
        for index,(count,present) in enumerate(zip(counts,wanted)):
            if (count>=50)!=present:
                raise RuntimeError(f"Native HUD eye={eye} site={index}: pixels={count}, expected={present}")
        results.append({"wrist_health":counts[0],"wrist_ammo":counts[1],"legacy_health":counts[2],"legacy_ammo":counts[3]})
    return results


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
    parser.add_argument("--wrist-panels", action="store_true", help="Require rendered prototype panels, not just pose snapshots")
    parser.add_argument("--native-hud", action="store_true", help="Capture original HUD artwork; marker-card assertions do not apply")
    parser.add_argument("--native-replace", action="store_true")
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
        if args.wrist_panels or args.native_hud:
            scenarios += [("left-moved",1,1,1),("left-rotated",1,1,1),("recentered",1,1,1)]
        baseline_panels = None
        for name, left, right, focused in scenarios:
            overrides_touched = True
            for hand, active, x in (("left", left, -180), ("right", right, 180)):
                # Cancel each mirrored inner-side attachment in these fixed ROIs.
                extra = {"qz":707107 if hand=="left" else -707107,"qw":707107} if args.native_hud else {}
                if hand == "left" and name == "left-moved": x = -80
                if hand == "left" and name == "left-rotated":
                    extra = {"qx":353553,"qy":353553,"qz":612372,"qw":612372} if args.native_hud else {"qy":500000,"qw":866025}
                command("openxr_hands_override", hand=hand, pose="grip",
                        active=active, focused=focused, flags=15,
                        px_mm=x, py_mm=-180, pz_mm=-450, **extra)
            if name == "recentered": command("openxr_control",recenter=1)
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
            if args.wrist_panels or args.native_hud:
                expected = (left if focused else 0, right if focused else 0)
                if after.get("submitted_surfaces") != sum(expected) or after.get("surface_failures",0):
                    raise RuntimeError(f"Unexpected surface submission in {name}: {after}")
            if args.wrist_panels:
                capture["panels"] = panel_pixels(output/name/"compositor.png",expected)
                if name == "both-hands": baseline_panels = capture["panels"]
                if name == "left-moved":
                    for eye in range(2):
                        if capture["panels"][0][eye]["centroid"][0]-baseline_panels[0][eye]["centroid"][0] < 30:
                            raise RuntimeError("Rendered left panel did not follow translated grip")
                        if abs(capture["panels"][1][eye]["centroid"][0]-baseline_panels[1][eye]["centroid"][0]) > 5:
                            raise RuntimeError("Moving left grip moved the right panel")
                if name == "left-rotated":
                    for eye in range(2):
                        bounds=capture["panels"][0][eye]["bounds"]
                        base=baseline_panels[0][eye]["bounds"]
                        if abs((bounds[2]-bounds[0])-(base[2]-base[0])) < 10:
                            raise RuntimeError("Rendered left panel did not rotate with grip")
                if name == "recentered":
                    for side in range(2):
                        for eye in range(2):
                            if max(abs(a-b) for a,b in zip(capture["panels"][side][eye]["centroid"],baseline_panels[side][eye]["centroid"]))>5:
                                raise RuntimeError("Recenter drifted a panel in the fixed-origin null-HMD control")
            if args.native_hud:
                capture["native_hud_pixels"]=native_hud_pixels(output/name/"compositor.png",expected,args.native_replace)
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
        result["prototype_panels_verified"] = args.wrist_panels
        result["native_art_capture_verified"] = args.native_hud
        result["native_art_and_replacement_pixels_verified"] = args.native_hud and args.native_replace
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
