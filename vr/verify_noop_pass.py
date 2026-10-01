#!/usr/bin/env python3
"""Compare decoded baseline/pass pixels in a render_pass_dump directory.

Requires Pillow. Reports actual pixel equality, not screenshot-hash inference.
Usage: python vr/verify_noop_pass.py <dump-dir> --output <receipt.json>
"""
import argparse
import json
import re
from pathlib import Path

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    comparisons = []
    for path in sorted(args.directory.glob("g*_*.png")):
        match = re.fullmatch(r"(g\d+)_\d+_a(\d+)\.png", path.name)
        if not match or int(match[2]) == 0:
            continue
        baseline = args.directory / (match[1] + "_00_a00000.png")
        with Image.open(baseline) as base, Image.open(path) as capture:
            base_rgb, capture_rgb = base.convert("RGB"), capture.convert("RGB")
            same_size = base_rgb.size == capture_rgb.size
            left, right = base_rgb.tobytes(), capture_rgb.tobytes()
            changed = (sum(left[i:i+3] != right[i:i+3] for i in range(0, len(left), 3))
                       if same_size else None)
            comparisons.append({"baseline": baseline.name, "pass": path.name,
                                "baseline_size": list(base_rgb.size),
                                "pass_size": list(capture_rgb.size),
                                "changed_pixels": changed,
                                "equal": same_size and changed == 0})
    receipt = {"method": "decoded RGB pixel comparison", "comparisons": comparisons,
               "ok": bool(comparisons) and all(item["equal"] for item in comparisons)}
    result = json.dumps(receipt, indent=2) + "\n"
    if args.output:
        args.output.write_text(result, encoding="utf-8")
    print(result, end="")
    return 0 if receipt["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
