"""Measure the clear-only and level-only controls in a scene-replay bundle."""
import argparse
import json
from PIL import Image, ImageChops
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    comparisons = []
    for mode in ("clear", "level"):
        a = Image.open(args.bundle / mode / "g000_00_a00000.png").convert("RGB")
        b = Image.open(args.bundle / mode / "g000_01_a32768.png").convert("RGB")
        if a.size != (512, 240) or b.size != a.size:
            raise ValueError("Expected matching 512x240 captures")
        row = {"mode": mode, "size": list(b.size), "all_black": b.getbbox() is None,
               "changed_pixels": sum(x != y for x, y in zip(a.getdata(), b.getdata())),
               "diff_bbox": ImageChops.difference(a, b).getbbox(), "row_bands": []}
        for lo, hi in ((0, 30), (30, 120), (120, 180), (180, 240)):
            box = (0, lo, 512, hi)
            row["row_bands"].append({"rows": [lo, hi], "changed": sum(
                x != y for x, y in zip(a.crop(box).getdata(), b.crop(box).getdata()))})
        comparisons.append(row)
    result = {"comparisons": comparisons}
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if comparisons[0]["all_black"] and not comparisons[1]["all_black"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
