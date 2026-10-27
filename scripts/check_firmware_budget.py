#!/usr/bin/env python3
"""Fail CI when PlatformIO firmware usage exceeds approved budgets."""
from pathlib import Path
import argparse
import re
import sys

RAM_BUDGET_BYTES = 9216
FLASH_BUDGET_BYTES = 28672
USAGE_PATTERN = re.compile(
    r"^(RAM|Flash):.*used\s+(\d+)\s+bytes\s+from\s+(\d+)\s+bytes",
    re.MULTILINE,
)


def parse_usage(text):
    usage = {}
    for name, used, total in USAGE_PATTERN.findall(text):
        usage[name.lower()] = (int(used), int(total))
    return usage


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_log", type=Path)
    parser.add_argument("--ram-budget", type=int, default=RAM_BUDGET_BYTES)
    parser.add_argument("--flash-budget", type=int, default=FLASH_BUDGET_BYTES)
    args = parser.parse_args()

    if not args.build_log.is_file():
        print(f"ERROR: build log not found: {args.build_log}")
        return 2

    usage = parse_usage(args.build_log.read_text(encoding="utf-8", errors="replace"))
    missing = [name for name in ("ram", "flash") if name not in usage]
    if missing:
        print("ERROR: missing memory usage: " + ", ".join(missing))
        return 2

    ram_used, ram_total = usage["ram"]
    flash_used, flash_total = usage["flash"]
    failures = []

    if ram_used > args.ram_budget:
        failures.append(f"RAM {ram_used} > {args.ram_budget} bytes")
    if flash_used > args.flash_budget:
        failures.append(f"Flash {flash_used} > {args.flash_budget} bytes")

    print("=" * 68)
    print("FIRMWARE MEMORY BUDGET")
    print("=" * 68)
    print(f"RAM   : {ram_used} / {ram_total} bytes, budget {args.ram_budget}")
    print(f"Flash : {flash_used} / {flash_total} bytes, budget {args.flash_budget}")

    if failures:
        print("Status: FAILED")
        for failure in failures:
            print("- " + failure)
        return 1

    print("Status: PASSED")
    print(f"RAM headroom   : {args.ram_budget - ram_used} bytes")
    print(f"Flash headroom : {args.flash_budget - flash_used} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
