"""Compare decoded eye pixels and explicit horizontal ROI correspondences.

NCC reports a measured image correspondence, not a world-depth estimate.
Inspect the named ROIs before interpreting their displacement as near/far.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def correspondence(left, right, box, limit=48):
    x0, y0, x1, y1 = box
    reference = left[y0:y1, x0:x1].astype(float).mean(axis=2)
    reference -= reference.mean()
    candidates = []
    for shift in range(-limit, limit + 1):
        if x0 + shift < 0 or x1 + shift > right.shape[1]:
            continue
        candidate = right[y0:y1, x0 + shift:x1 + shift].astype(float).mean(axis=2)
        candidate -= candidate.mean()
        denominator = np.linalg.norm(reference) * np.linalg.norm(candidate)
        if denominator:
            candidates.append((float((reference * candidate).sum() / denominator), shift))
    score, shift = max(candidates)
    return {"box": box, "right_minus_left_px": shift, "ncc": score}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--expect", choices=("equal", "different"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    results = []
    for path in sorted(args.directory.glob("p*.json")):
        if not path.stem[1:].isdigit():
            continue
        manifest = json.loads(path.read_text(encoding="utf-8-sig"))
        left = np.asarray(Image.open(path.with_name(path.stem + "_left.png")).convert("RGB"))
        right = np.asarray(Image.open(path.with_name(path.stem + "_right.png")).convert("RGB"))
        scale = left.shape[1] // 512
        if (not scale or left.shape != right.shape or
            left.shape[:2] != (240 * scale, 512 * scale)):
            raise ValueError("Expected matching integer-scaled 512x240 eyes")
        changed = np.any(left != right, axis=2)
        row = {"manifest": path.name, **manifest, "changed_pixels": int(changed.sum()),
               "roi_grid_sample_step": scale,
               "row_bands": [{"display_rows": [lo, hi], "changed_pixels": int(changed[lo*scale:hi*scale].sum())}
                             for lo, hi in ((0, 30), (30, 120), (120, 180), (180, 240))],
               "roi_correspondences": {name: correspondence(left[::scale, ::scale], right[::scale, ::scale], box)
                  for name, box in {"wall": [50, 100, 170, 130],
                                    "ground": [50, 195, 200, 230]}.items()}}
        results.append(row)
    ok = bool(results) and all((r["changed_pixels"] == 0) == (args.expect == "equal")
                               for r in results)
    args.output.write_text(json.dumps({"expect": args.expect, "ok": ok, "pairs": results},
                                     indent=2) + "\n", encoding="utf-8")
    for row in results:
        print(row["manifest"], "changed:", row["changed_pixels"], row["roi_correspondences"])
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
