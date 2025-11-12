import re
import sys

# Compile regex once for better performance
PATTERN = re.compile(r"avg=(\d+).*min=(\d+).*max=(\d+).*count=(\d+)")

# Get filename from command line or use default
filename = sys.argv[1] if len(sys.argv) > 1 else "period_log.txt"

try:
    with open(filename) as f:
        for line in f:
            match = PATTERN.search(line)
            if match:
                avg, min_val, max_val, count = map(int, match.groups())
                print(f"avg={avg}us  min={min_val}us  max={max_val}us  count={count}")
except FileNotFoundError:
    print(f"Error: File '{filename}' not found")
    sys.exit(1)
