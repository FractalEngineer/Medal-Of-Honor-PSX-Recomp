"""Fail-closed checks for the unattended XR verification harness."""
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image

from headless_steamvr import Compositor, null_settings
from check_headless_mirror import check_progress
from capture_stereo import collect_pair_manifests


class HeadlessMirrorTests(unittest.TestCase):
    def test_existing_disabled_null_is_enabled_and_unrelated_settings_survive(self):
        original = {"driver_null": {"enable": False, "renderWidth": 900},
                    "steamvr": {"forcedDriver": "other", "enableHomeApp": True},
                    "audio": {"enableSpatializeGlobal": True}}
        updated = json.loads(null_settings(json.dumps(original)))
        self.assertTrue(updated["driver_null"]["enable"])
        self.assertEqual(updated["driver_null"]["renderWidth"], 900)
        self.assertEqual(updated["steamvr"]["forcedDriver"], "null")
        self.assertFalse(updated["steamvr"]["enableHomeApp"])
        self.assertFalse(updated["dashboard"]["enableDashboard"])
        self.assertFalse(updated["power"]["pauseCompositorOnStandby"])
        self.assertEqual(updated["audio"], original["audio"])
        self.assertFalse(original["driver_null"]["enable"])

    def test_malformed_settings_refused(self):
        for value in ("[]", '{"steamvr": false}', '{"driver_null": 1}', "{"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                null_settings(value)

    def test_live_projection_required(self):
        before = {"submitted": 100, "failures": 0, "state": 5}
        valid = {"running": 1, "tracking": 1, "submitted": 101,
                 "submitted_layer": 1, "submitted_source": 1, "failures": 0, "state": 5}
        check_progress(before, valid)
        for field, value in (("submitted", 100), ("running", 0), ("tracking", 0),
                             ("submitted_layer", 2), ("submitted_source", 2),
                             ("failures", 1), ("state", 4)):
            with self.subTest(field=field), self.assertRaises(RuntimeError):
                check_progress(before, {**valid, field: value})

    def test_capture_rejects_other_app_and_accepts_only_new_pngs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "screenshots").mkdir()
            compositor = object.__new__(Compositor)
            compositor.runtime = root
            compositor.renderer_pid = lambda: 42
            def dump():
                image = Image.new("RGB", (32, 16), "black")
                image.paste("white", (4, 4, 12, 12))
                for suffix in ("", "-raw"):
                    image.save(root / "screenshots" / f"fresh{suffix}.png")
            compositor.dump_images = dump
            with self.assertRaises(RuntimeError):
                compositor.capture(root / "wrong-app", 99)
            self.assertFalse(list((root / "wrong-app").glob("*.png")))
            record = compositor.capture(root / "valid", 42)
            self.assertEqual(record["renderer_pid_after"], 42)
            self.assertEqual(len(record["images"]), 2)
            self.assertFalse(record["atomic_guest_pair"])

    def test_pair_metadata_requires_complete_matching_images(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            record = {"pair_id": 7, "width": 32, "height": 16}
            Image.new("RGB", (32, 16)).save(root / "p000007_left.png")
            collect_pair_manifests(root, [record])
            self.assertFalse((root / "p000007.json").exists())
            Image.new("RGB", (32, 16)).save(root / "p000007_right.png")
            Image.new("RGB", (64, 16)).save(root / "p000007_sbs.png")
            collect_pair_manifests(root, [record])
            self.assertEqual(json.loads((root / "p000007.json").read_text()), record)
            Image.new("RGB", (30, 16)).save(root / "p000007_right.png")
            with self.assertRaises(RuntimeError):
                collect_pair_manifests(root, [record])


if __name__ == "__main__":
    unittest.main()
