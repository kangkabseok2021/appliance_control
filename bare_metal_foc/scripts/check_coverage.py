#!/usr/bin/env python3
"""Parse lcov HTML report and assert 100% line + >= 90% branch coverage.
Usage: python3 scripts/check_coverage.py docs/coverage/index.html
"""
import sys
import re


def parse_coverage(html_path: str) -> tuple[float, float]:
    """Return (line_pct, branch_pct) from lcov HTML index."""
    with open(html_path) as f:
        text = f.read()
    # lcov HTML has patterns like: "90.0 %" or "100.0 %"
    pcts = re.findall(r'(\d+\.\d+)\s*%', text)
    if len(pcts) < 2:
        print(f"WARNING: could not parse coverage from {html_path}")
        return 0.0, 0.0
    # First occurrence is usually line coverage, second is branch
    return float(pcts[0]), float(pcts[1])


def main() -> int:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <lcov-html-index>")
        return 1

    html_path = sys.argv[1]
    line_pct, branch_pct = parse_coverage(html_path)
    print(f"Line coverage:   {line_pct:.1f}%")
    print(f"Branch coverage: {branch_pct:.1f}%")

    ok = True
    if line_pct < 100.0:
        print(f"FAIL: line coverage {line_pct:.1f}% < 100% (REQ-006)")
        ok = False
    if branch_pct < 90.0:
        print(f"FAIL: branch coverage {branch_pct:.1f}% < 90% (REQ-006)")
        ok = False
    if ok:
        print("PASS: coverage requirements met (REQ-006)")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
