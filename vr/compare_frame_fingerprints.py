"""Compare equal post-load windows; host frame labels may differ across loads."""
import argparse
import json
from pathlib import Path

JUDGE = ("wc", "ws", "mmio", "mc", "sp", "sc", "qc", "cyc")
LOCATOR = ("wr", "pc")


def compare(baseline, candidate, count):
    a, b = [json.loads(p.read_text(encoding="utf-8-sig"))["entries"]
            for p in (baseline, candidate)]
    if len(a) < count or len(b) < count:
        raise ValueError(f"Need {count} entries: {baseline}={len(a)}, {candidate}={len(b)}")
    for entries in (a, b):
        labels = [e["frame"] for e in entries[:count]]
        if any(y != x + 1 for x, y in zip(labels, labels[1:])):
            raise ValueError("Window must contain consecutive guest frames")
    differences = []
    for i, (x, y) in enumerate(zip(a[:count], b[:count])):
        changed = {k: [x[k], y[k]] for k in JUDGE + LOCATOR if x[k] != y[k]}
        if changed:
            differences.append({"relative_frame": i, "columns": changed})
    return {"baseline": str(baseline), "candidate": str(candidate),
            "frames": count, "baseline_first_label": a[0]["frame"],
            "candidate_first_label": b[0]["frame"],
            "judge_columns": list(JUDGE), "locator_columns": list(LOCATOR),
            "equal": not differences, "differences": differences}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidates", nargs="+", type=Path)
    parser.add_argument("--count", type=int, default=96)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.count < 1:
        parser.error("count must be positive")
    result = {"comparisons": [compare(args.baseline, p, args.count)
                              for p in args.candidates]}
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for row in result["comparisons"]:
        print(f"{row['candidate']}: {row['frames']} frames, equal={row['equal']}")
    return 0 if all(row["equal"] for row in result["comparisons"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
