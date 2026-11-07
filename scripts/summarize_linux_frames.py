"""Summarize an explicitly selected, visually verified gameplay frame window."""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parents[1]


def percentile(values, fraction):
    index = (len(values) - 1) * fraction
    low = math.floor(index)
    high = math.ceil(index)
    return values[low] + (values[high] - values[low]) * (index - low)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('tag_or_csv')
    ap.add_argument('--first-frame', type=int, required=True)
    ap.add_argument('--last-frame', type=int, required=True)
    ap.add_argument('--exclude-captures', action='store_true')
    args = ap.parse_args()
    if args.first_frame < 0 or args.last_frame <= args.first_frame:
        ap.error('select a nonempty frame window [first, last)')
    path = Path(args.tag_or_csv)
    if path.suffix != '.csv':
        path = ROOT / 'analysis' / (args.tag_or_csv + '.frames.csv')
    with path.open(newline='') as source:
        rows = [r for r in csv.DictReader(source)
                if args.first_frame <= int(r['frame']) < args.last_frame]
    excluded_captures = 0
    if args.exclude_captures:
        if rows and 'capture' not in rows[0]:
            ap.error('this CSV does not identify capture frames')
        excluded_captures = sum(int(r['capture']) != 0 for r in rows)
        rows = [r for r in rows if not int(r['capture'])]
    values = sorted(float(r['interval_ms']) for r in rows)
    if not values or any(not math.isfinite(v) or v <= 0 for v in values):
        ap.error('window has no valid positive frame intervals')
    slowest = values[-max(1, math.ceil(len(values) * .01)):]
    result = dict(first_frame=args.first_frame, last_frame_exclusive=args.last_frame,
                  frames=len(values), seconds=sum(values) / 1000,
                  excluded_capture_frames=excluded_captures,
                  mean_fps=1000 / statistics.mean(values),
                  one_percent_low_fps=1000 / statistics.mean(slowest),
                  median_ms=statistics.median(values), p95_ms=percentile(values, .95),
                  p99_ms=percentile(values, .99), maximum_ms=max(values),
                  over_16_67_ms=sum(v > 1000 / 60 for v in values),
                  over_33_33_ms=sum(v > 1000 / 30 for v in values),
                  mean_draws=statistics.mean(int(r['draws']) for r in rows))
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
