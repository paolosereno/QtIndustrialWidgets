#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Checks the skips of a CI test run against tests/expected_skips.json (#274).

  tools/ci_skips.py --platform PLATFORM LOG [--expected-skips FILE]

LOG is the output of `ctest -V` (every test binary prints its QtTest lines, each prefixed with the ctest test number).
Each ctest test runs at the scale of its name (tests/CMakeLists.txt): <name> at 1, <name>_dpr125 at 1.25,
<name>_dpr150 at 1.5 and <name>_hidpi at 2. A SKIP line of a test at scale S is expected if an entry of the expected
skips file lists its test binary, its function, a data tag regular expression it matches in full and the configuration
"PLATFORM S" (for example "windows ci 1.25", tools/run_configs.py CI_LABELS). Any other skip fails the check, and so
does a log with no test result.

Exit status: 0 every skip is expected; 1 setup error (no log, an invalid expected skips file, an unknown platform);
2 at least one skip that no entry lists, or no test ran.
"""

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_configs import CI_PLATFORMS, EXPECTED_SKIPS, is_expected_skip, load_expected_skips  # noqa: E402

# "  3/141 Test   #3: tst_Foo_dpr125 ...................   Passed    0.02 sec"
RESULT = re.compile(r"^\s*\d+/\d+ Test\s+#(\d+): (\S+) ")
# "3: SKIP   : tst_Foo::bar(row) reason"
OUTPUT = re.compile(r"^(\d+): (.*)$")
SUFFIXES = (("_dpr125", "1.25"), ("_dpr150", "1.5"), ("_hidpi", "2"))


def scale_of(name):
    """The QT_SCALE_FACTOR of the ctest test \a name (tests/CMakeLists.txt)."""
    for suffix, scale in SUFFIXES:
        if name.endswith(suffix):
            return scale
    return "1"


def check(log_text, platform, expected):
    """(expected skips, unexpected SKIP lines with their configuration, tests run) of a `ctest -V` log."""
    names = {}
    for line in log_text.splitlines():
        m = RESULT.match(line)
        if m:
            names[m.group(1)] = m.group(2)
    found = 0
    unexpected = []
    for line in log_text.splitlines():
        m = OUTPUT.match(line)
        if not m or not m.group(2).startswith("SKIP"):
            continue
        number, text = m.groups()
        label = f"{platform} {scale_of(names.get(number, ''))}"
        if number in names and is_expected_skip(expected, label, text):
            found += 1
        else:
            unexpected.append(f"[{label}] {names.get(number, 'test #' + number)}: {text}")
    return found, unexpected, len(names)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("log", type=Path, help="output of ctest -V")
    parser.add_argument("--platform", required=True, help=f"one of {list(CI_PLATFORMS)}")
    parser.add_argument("--expected-skips", type=Path, default=Path(__file__).resolve().parent.parent / EXPECTED_SKIPS)
    args = parser.parse_args()
    if args.platform not in CI_PLATFORMS:
        print(f"ci_skips: unknown platform {args.platform!r}; known: {list(CI_PLATFORMS)}", file=sys.stderr)
        return 1
    if not args.log.is_file():
        print(f"ci_skips: no log {args.log}", file=sys.stderr)
        return 1
    try:
        expected = load_expected_skips(args.expected_skips)
    except (ValueError, json.JSONDecodeError) as e:
        print(f"ci_skips: invalid expected skips file: {e}", file=sys.stderr)
        return 1
    found, unexpected, tests = check(args.log.read_text(encoding="utf-8", errors="replace"), args.platform, expected)
    if tests == 0:
        print("ci_skips: no test result in the log", file=sys.stderr)
        return 2
    print(f"ci_skips: {tests} tests, {found} expected skips, {len(unexpected)} not listed")
    for line in unexpected:
        print(f"  {line}")
    if unexpected:
        print(f"ci_skips: {len(unexpected)} skips are not in {args.expected_skips}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
