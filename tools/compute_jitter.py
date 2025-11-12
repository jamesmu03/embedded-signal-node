#!/usr/bin/env python3
"""
Parse lines like:
period_cycles: avg=XXXXX min=YYYYY max=ZZZZZ count=NNNN | us: avg=AAA min=BBB max=CCC

Produces statistics on cycles and microseconds.
Usage:
  python3 tools/compute_jitter_cycles.py docs/period_log.txt
"""

import re
import sys
from statistics import mean, stdev

PAT = re.compile(r"period_cycles:\s+avg=(\d+)\s+min=(\d+)\s+max=(\d+)\s+count=(\d+).*us:\s+avg=(\d+)\s+min=(\d+)\s+max=(\d+)")

def parse_file(fn):
    cycles = []
    us = []
    try:
        with open(fn) as f:
            for line in f:
                m = PAT.search(line)
                if m:
                    avg_c = int(m.group(1))
                    min_c = int(m.group(2))
                    max_c = int(m.group(3))
                    cnt = int(m.group(4))
                    avg_us = int(m.group(5))
                    min_us = int(m.group(6))
                    max_us = int(m.group(7))
                    cycles.append((avg_c, min_c, max_c, cnt))
                    us.append((avg_us, min_us, max_us, cnt))
    except FileNotFoundError:
        print(f"Error: file not found: {fn}")
        sys.exit(1)

    return cycles, us

def report(cycles, us):
    if not cycles:
        print("No period_cycles lines found.")
        return
    print("Per-sample window (cycles -> us):")
    for i, ((a_c, mi_c, ma_c, cnt), (a_u, mi_u, ma_u, _)) in enumerate(zip(cycles, us), 1):
        print(f"{i}: avg_cycles={a_c} min_cycles={mi_c} max_cycles={ma_c} count={cnt} | avg_us={a_u} min_us={mi_u} max_us={ma_u}")

    avg_cycles_list = [c[0] for c in cycles]
    avg_us_list = [u[0] for u in us]
    print()
    print(f"Summary over {len(cycles)} windows:")
    print(f" cycles: mean={mean(avg_cycles_list):.2f}  std={stdev(avg_cycles_list):.2f}")
    print(f"   us  : mean={mean(avg_us_list):.2f}  std={stdev(avg_us_list):.2f}")

if __name__ == "__main__":
    fn = sys.argv[1] if len(sys.argv) > 1 else "docs/period_log.txt"
    cycles, us = parse_file(fn)
    report(cycles, us)
