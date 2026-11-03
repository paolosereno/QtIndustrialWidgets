#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Runs test binaries in the 8 configurations; any failing or skipped configuration fails the whole run.

  tools/run_configs.py TEST [TEST ...] [--build-dir build] [--markdown FILE] [--jobs N] [--timeout SECONDS]

TEST is a binary of BUILD_DIR/bin (tst_ValveIndicator, ...); the build is used as it is, nothing is rebuilt.

The 8 configurations: QT_SCALE_FACTOR 1, 1.25, 1.5 and 2, each with the system fonts and with no font at all
(nofont-env.sh next to this script, which fails if fc-list still sees a font). QT_QPA_PLATFORM=offscreen.
The repository is the one of the current directory, so the script can also run on another worktree.

A configuration passes only if every binary ran to its "Totals:" line with at least one passed function, no FAIL!,
nothing skipped (QSKIP) or blacklisted, no crash and no timeout. The configuration without fonts is never left out:
if its environment cannot be set up, nothing runs and the exit status is 1.

Exit status: 0 every binary passed in all 8 configurations; 1 setup error (a missing binary, no environment without
fonts); 2 at least one configuration failed, crashed, timed out, skipped a test function or ran nothing.

mutation_check.py takes LABELS, configurations() and run_all() from here, so that both count a run the same way.
"""

import argparse
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

SCALES = ("1", "1.25", "1.5", "2")
LABELS = tuple(f"fonts {s}" for s in SCALES) + tuple(f"no fonts {s}" for s in SCALES)
CRASH = 1000
TOTALS = re.compile(r"^Totals: (\d+) passed, (\d+) failed, (\d+) skipped(?:, (\d+) blacklisted)?")


def fail(message):
    print(f"run_configs: {message}", file=sys.stderr)
    sys.exit(1)


@dataclass
class Result:
    """Outcome of the test binaries in one configuration."""
    passed: int = 0
    fails: int = 0      # FAIL! lines; a crash, a timeout or a missing "Totals:" counts CRASH
    skipped: int = 0    # skipped and blacklisted test functions
    empty: int = 0      # binaries that passed no test function
    names: set = field(default_factory=set)

    def ok(self):
        return self.fails == 0 and self.skipped == 0 and self.empty == 0


def no_font_config(directory):
    """FONTCONFIG_FILE of an environment without fonts, from nofont-env.sh next to this script.

    Raises RuntimeError if the environment cannot be set up (fc-list missing, or a font still visible)."""
    script = Path(__file__).resolve().parent / "nofont-env.sh"
    r = subprocess.run(["bash", "-c", 'source "$1" --dir "$2" > /dev/null && printf "%s" "$FONTCONFIG_FILE"', "_",
                        str(script), str(directory)], capture_output=True, text=True)
    if r.returncode != 0 or not r.stdout:
        raise RuntimeError(f"{script} failed:\n{r.stderr.strip()}")
    return r.stdout


def run_tests(bin_dir, tests, scale, fontconfig, timeout):
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_SCALE_FACTOR=scale)
    env.pop("FONTCONFIG_FILE", None)
    if fontconfig:
        env["FONTCONFIG_FILE"] = fontconfig
    result = Result()
    for test in tests:
        try:
            out = subprocess.run([str(bin_dir / test)], env=env, capture_output=True, text=True,
                                 timeout=timeout).stdout
        except subprocess.TimeoutExpired:
            result.fails += CRASH
            result.names.add(f"{test} timed out")
            continue
        totals = None
        for line in out.splitlines():
            if line.startswith("FAIL!"):
                result.fails += 1
                result.names.add(line.split("::", 1)[1].split("(", 1)[0] if "::" in line else line)
            elif line.startswith(("SKIP", "BLACKLISTED", "BPASS", "BFAIL")):
                result.names.add(f"{test} skipped: " + (line.split("::", 1)[1].split("(", 1)[0]
                                                         if "::" in line else line))
            elif totals is None:
                totals = TOTALS.match(line)
        if totals is None:
            result.fails += CRASH
            result.names.add(f"{test} crashed")
            continue
        result.passed += int(totals.group(1))
        result.skipped += int(totals.group(3)) + int(totals.group(4) or 0)
        if int(totals.group(1)) == 0:
            result.empty += 1
            result.names.add(f"{test} ran nothing")
    return result


def configurations(nofont_dir):
    """The 8 configurations as (scale, FONTCONFIG_FILE or None), in the order of LABELS."""
    nofont = no_font_config(nofont_dir)
    return [(scale, None) for scale in SCALES] + [(scale, nofont) for scale in SCALES]


def run_all(bin_dir, tests, configs, jobs, timeout):
    with concurrent.futures.ThreadPoolExecutor(max(1, jobs)) as pool:
        return list(pool.map(lambda c: run_tests(bin_dir, tests, c[0], c[1], timeout), configs))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("tests", nargs="+", help="test binaries of BUILD_DIR/bin")
    parser.add_argument("--build-dir", type=Path, default=Path("build"), help="relative to the repository root")
    parser.add_argument("--markdown", type=Path, help="also write the result table to this file")
    parser.add_argument("--jobs", type=int, default=len(LABELS), help="configurations run in parallel")
    parser.add_argument("--timeout", type=int, default=1800, help="seconds per test binary and configuration")
    args = parser.parse_args()

    top = subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True)
    if top.returncode != 0:
        fail("not inside a git repository")
    bin_dir = (Path(top.stdout.strip()) / args.build_dir).resolve() / "bin"
    for t in args.tests:
        if not (bin_dir / t).is_file():
            fail(f"test binary {bin_dir / t} not found (build first)")

    nofont_dir = Path(tempfile.mkdtemp(prefix="qiw-configs-nofonts."))
    try:
        try:
            configs = configurations(nofont_dir)
        except RuntimeError as e:
            fail(f"no environment without fonts, nothing was run: {e}")
        results = run_all(bin_dir, args.tests, configs, args.jobs, args.timeout)
    finally:
        shutil.rmtree(nofont_dir, ignore_errors=True)

    lines = ["| Configuration | Passed | Failed | Skipped | Result | Problems |", "|---|---|---|---|---|---|"]
    for label, r in zip(LABELS, results):
        status = "PASS" if r.ok() else "FAIL"
        problems = ", ".join(sorted(r.names))
        print(f"{label:>14}: {status}  passed {r.passed}, failed {r.fails}, skipped {r.skipped}"
              + (f"  | {problems}" if problems else ""), flush=True)
        lines.append(f"| {label} | {r.passed} | {r.fails} | {r.skipped} | {status} | {problems} |")
    if args.markdown:
        args.markdown.write_text("\n".join(lines) + "\n", encoding="utf-8")

    bad = [label for label, r in zip(LABELS, results) if not r.ok()]
    print(f"tests: {' '.join(args.tests)}")
    if bad:
        print(f"run_configs: {len(bad)} of {len(LABELS)} configurations did not pass: {', '.join(bad)}",
              file=sys.stderr)
        return 2
    print(f"all {len(LABELS)} configurations passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
