#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Sabotage (mutation) check: applies each sabotage, rebuilds, runs the tests in 8 configurations, restores.

  tools/mutation_check.py SABOTAGES.json [--build-dir build] [--only NAME ...] [--require-detection]
                          [--markdown FILE] [--jobs N] [--timeout SECONDS]

SABOTAGES.json (paths relative to the repository root):

  {
    "tests": ["tst_TankDisplay", "tst_TankDisplayLayout"],
    "sabotages": [
      {"name": "T1 high limit entered with >",
       "edits": [{"file": "src/TankDisplay.cpp", "old": "raw >= l[i] ||", "new": "raw > l[i] ||"}]},
      {"name": "T2 ...", "edits": [...], "tests": ["tst_ReadoutContrast"]}
    ]
  }

"tests" are binaries of BUILD_DIR/bin; a sabotage may name its own. Each "old" must occur exactly once in its file
(after the earlier edits of the same sabotage), so that a sabotage never lands somewhere else silently.

The 8 configurations: QT_SCALE_FACTOR 1, 1.25, 1.5 and 2, each with the system fonts and with no font at all
(nofont-env.sh next to this script, which fails if fc-list still sees a font). QT_QPA_PLATFORM=offscreen.
The repository is the one of the current directory, so the script can also run on another worktree.

For each sabotage, in order:
  1. the edits are written;
  2. the objects of the touched .cpp files are removed (the WSL clock can go back, so that make or ninja would skip
     a rebuild); a touched file of another kind (a header) rebuilds everything with --clean-first;
  3. cmake --build; a sabotage that does not compile is reported as BUILD FAILED;
  4. every test binary runs in the 8 configurations; the result is the number of FAIL! lines per configuration
     (a crash, a timeout or a missing "Totals:" counts 1000) and the names of the failing test functions;
  5. the sources are restored with git checkout HEAD -- <files>, also on an error or Ctrl+C.
A baseline without edits runs first: if it fails anywhere, the results would mean nothing and the script stops.
At the end, git diff --exit-code on every touched file checks that the sources are back to HEAD.

The files to sabotage must have no local change (it would be lost on the restore): commit or save it first.
After the run, CLAUDE.md rule 8 still applies: cmake --build BUILD_DIR --clean-first and a full ctest before any
measurement or image.

Exit status: 0 done; 1 setup error, a missing "old", or a failing baseline; 2 with --require-detection, a sabotage
missed in at least one configuration (or not compiled); 3 the sources differ from HEAD after the restore.
"""

import argparse
import concurrent.futures
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SCALES = ("1", "1.25", "1.5", "2")
CRASH = 1000


def fail(message):
    print(f"mutation_check: {message}", file=sys.stderr)
    sys.exit(1)


def git(root, *args, check=True):
    return subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True, check=check)


def no_font_config(directory):
    """FONTCONFIG_FILE of an environment without fonts, from nofont-env.sh next to this script."""
    script = Path(__file__).resolve().parent / "nofont-env.sh"
    r = subprocess.run(["bash", "-c", 'source "$1" "$2" > /dev/null && printf "%s" "$FONTCONFIG_FILE"', "_",
                        str(script), str(directory)], capture_output=True, text=True)
    if r.returncode != 0 or not r.stdout:
        fail(f"{script} failed:\n{r.stderr.strip()}")
    return r.stdout


def run_tests(bin_dir, tests, scale, fontconfig, timeout):
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_SCALE_FACTOR=scale)
    env.pop("FONTCONFIG_FILE", None)
    if fontconfig:
        env["FONTCONFIG_FILE"] = fontconfig
    fails, names = 0, set()
    for test in tests:
        try:
            out = subprocess.run([str(bin_dir / test)], env=env, capture_output=True, text=True,
                                 timeout=timeout).stdout
        except subprocess.TimeoutExpired:
            fails += CRASH
            names.add(f"{test} timed out")
            continue
        for line in out.splitlines():
            if line.startswith("FAIL!"):
                fails += 1
                names.add(line.split("::", 1)[1].split("(", 1)[0] if "::" in line else line)
        if "Totals:" not in out:
            fails += CRASH
            names.add(f"{test} crashed")
    return fails, names


def apply_edits(root, sabotage):
    """New contents of the touched files; fails if an "old" does not occur exactly once."""
    contents = {}
    for edit in sabotage.get("edits", []):
        path = root / edit["file"]
        text = contents.get(path)
        if text is None:
            text = path.read_text(encoding="utf-8")
        count = text.count(edit["old"])
        if count != 1:
            fail(f'{sabotage["name"]}: "{edit["old"][:60]}" occurs {count} times in {edit["file"]}, not once')
        contents[path] = text.replace(edit["old"], edit["new"])
    return contents


def rebuild(build_dir, touched):
    clean_first = any(p.suffix not in (".cpp", ".cc", ".cxx") for p in touched)
    if not clean_first:
        for p in touched:
            for obj in build_dir.rglob(p.name + ".o"):
                obj.unlink()
            for obj in build_dir.rglob(p.name + ".obj"):
                obj.unlink()
    cmd = ["cmake", "--build", str(build_dir)] + (["--clean-first"] if clean_first else [])
    return subprocess.run(cmd, capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sabotages", type=Path, help="JSON file with the tests and the sabotages")
    parser.add_argument("--build-dir", type=Path, default=Path("build"), help="relative to the repository root")
    parser.add_argument("--only", nargs="+", default=[], help="run only the sabotages whose name starts with these")
    parser.add_argument("--require-detection", action="store_true",
                        help="exit 2 if a sabotage is missed in a configuration")
    parser.add_argument("--markdown", type=Path, help="also write the result table to this file")
    parser.add_argument("--jobs", type=int, default=len(SCALES) * 2, help="configurations run in parallel")
    parser.add_argument("--timeout", type=int, default=1800, help="seconds per test binary and configuration")
    args = parser.parse_args()

    root = Path(git(Path.cwd(), "rev-parse", "--show-toplevel").stdout.strip())
    build_dir = (root / args.build_dir).resolve()
    bin_dir = build_dir / "bin"
    spec = json.loads(args.sabotages.read_text(encoding="utf-8"))
    default_tests = spec.get("tests", [])
    sabotages = [s for s in spec.get("sabotages", [])
                 if not args.only or any(s["name"].startswith(o) for o in args.only)]
    if not sabotages:
        fail("no sabotage to run")
    for s in sabotages:
        for t in s.get("tests", default_tests):
            if not (bin_dir / t).is_file():
                fail(f'{s["name"]}: test binary {bin_dir / t} not found (build first)')
        if not s.get("tests", default_tests):
            fail(f'{s["name"]}: no test to run')

    touched_all = sorted({edit["file"] for s in sabotages for edit in s.get("edits", [])})
    dirty = git(root, "status", "--porcelain", "--", *touched_all).stdout.strip() if touched_all else ""
    if dirty:
        fail(f"local changes in files to sabotage (they would be lost on the restore):\n{dirty}")
    for s in sabotages:  # every "old" must be found before anything is touched
        apply_edits(root, s)

    nofont_dir = Path(tempfile.mkdtemp(prefix="qiw-mutation-nofonts."))
    configs = [(scale, None) for scale in SCALES] + [(scale, no_font_config(nofont_dir)) for scale in SCALES]
    labels = [f"fonts {s}" for s in SCALES] + [f"no fonts {s}" for s in SCALES]

    rows = []
    missed = False

    def run_one(name, sabotage):
        tests = sabotage.get("tests", default_tests)
        contents = apply_edits(root, sabotage)
        try:
            for path, text in contents.items():
                path.write_text(text, encoding="utf-8")
            build = rebuild(build_dir, list(contents))
            if build.returncode != 0:
                tail = (build.stdout + build.stderr).strip().splitlines()[-15:]
                print(f"{name}: BUILD FAILED\n  " + "\n  ".join(tail), flush=True)
                return None
            with concurrent.futures.ThreadPoolExecutor(max(1, args.jobs)) as pool:
                return list(pool.map(lambda c: run_tests(bin_dir, tests, c[0], c[1], args.timeout), configs))
        finally:
            if contents:
                git(root, "checkout", "HEAD", "--", *[str(p.relative_to(root)) for p in contents])

    try:
        baseline = run_one("baseline", {"name": "baseline", "edits": [], "tests": sorted(
            {t for s in sabotages for t in s.get("tests", default_tests)})})
        if baseline is None or any(r[0] for r in baseline):
            names = sorted(set().union(*[r[1] for r in baseline])) if baseline else []
            fail("the baseline (no sabotage) fails: " + (", ".join(names) or "build failed"))
        print("baseline: " + " ".join("0" for _ in configs), flush=True)
        for s in sabotages:
            result = run_one(s["name"], s)
            if result is None:
                rows.append((s["name"], None, "BUILD FAILED"))
                missed = True
                continue
            counts = [r[0] for r in result]
            names = ", ".join(sorted(set().union(*[r[1] for r in result])))
            missed = missed or any(c == 0 for c in counts)
            rows.append((s["name"], counts, names))
            print(f'{s["name"]}: {" ".join(str(c) for c in counts)}  | {names}', flush=True)
    finally:
        shutil.rmtree(nofont_dir, ignore_errors=True)
        restored = git(root, "diff", "--exit-code", "HEAD", "--", *touched_all, check=False) if touched_all else None

    if args.markdown:
        lines = ["| Sabotage | " + " | ".join(labels) + " | Caught by |",
                 "|---|" + "---|" * len(labels) + "---|"]
        for name, counts, names in rows:
            cells = [str(c) for c in counts] if counts else ["–"] * len(labels)
            lines.append(f"| {name} | " + " | ".join(cells) + f" | {names} |")
        args.markdown.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print("configurations: " + ", ".join(labels))
    if restored is not None and restored.returncode != 0:
        print("mutation_check: the sources differ from HEAD after the restore:\n" + restored.stdout, file=sys.stderr)
        return 3
    print("sources restored (git diff --exit-code: clean). Rule 8: rebuild with --clean-first and run the full ctest.")
    if args.require_detection and missed:
        print("mutation_check: at least one sabotage was missed in a configuration (or did not compile)",
              file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
