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
The configurations and the way a test binary is run and counted come from run_configs.py, next to this script.
The repository is the one of the current directory, so the script can also run on another worktree.

A baseline without edits runs first, on a build made with --clean-first: if it does not pass in all 8 configurations
as run_configs.py requires (no FAIL!, nothing skipped, no crash), the results would mean nothing and the script stops.
The SHA-256 of every file in BUILD_DIR/bin and BUILD_DIR/lib after that build is the reference of the clean sources.

For each sabotage, in order:
  1. the edits are written;
  2. the build: the objects of the touched .cpp files are removed (the WSL clock can go back, so that make or ninja
     would skip a rebuild); a touched file of another kind (a header) rebuilds everything with --clean-first;
     a sabotage that does not compile is reported as BUILD FAILED;
  3. every test binary runs in the 8 configurations; the result is the number of FAIL! lines per configuration
     (a crash, a timeout or a missing "Totals:" counts 1000) and the names of the failing test functions;
  4. the sources are restored, also on an error or Ctrl+C: the script writes back the bytes it read before the edits
     (not git checkout: when mtimes go back, git can take a sabotaged file of the same size for unchanged, #197);
  5. the build again, as in step 2, so that no object of the sabotage is left for the next one (#197); the files of
     BUILD_DIR/bin and BUILD_DIR/lib must then match the reference of the clean build, otherwise the script stops
     before the next sabotage.
At the end, the content of every touched file is compared with HEAD (git hash-object, not git's stat cache).

The files to sabotage must have no local change (it would be lost on the restore): commit or save it first.
After the run, CLAUDE.md rule 8 still applies: cmake --build BUILD_DIR --clean-first and a full ctest before any
measurement or image.

Exit status: 0 done; 1 setup error (a file to sabotage outside the repository, among others), a missing "old", or a
failing baseline; 2 with --require-detection, a sabotage
missed in at least one configuration (or not compiled); 3 the sources differ from HEAD after the restore; 4 the build
after a restore does not match the clean build (or does not compile), so the run stopped.
"""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_configs import LABELS, configurations, run_all  # noqa: E402  (next to this script)



def fail(message):
    print(f"mutation_check: {message}", file=sys.stderr)
    sys.exit(1)


def git(root, *args, check=True):
    return subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True, check=check)


def inside_root(root, name):
    """The resolved path of a file to sabotage; fails if it is outside the repository (absolute, "..", a link)."""
    path = (root / name).resolve()
    if not path.is_relative_to(root):
        fail(f'"{name}" is outside the repository {root}: only files of the repository can be sabotaged')
    return path


def apply_edits(root, sabotage):
    """New contents of the touched files; fails if an "old" does not occur exactly once."""
    contents = {}
    for edit in sabotage.get("edits", []):
        path = inside_root(root, edit["file"])
        text = contents.get(path)
        if text is None:
            text = path.read_text(encoding="utf-8")
        count = text.count(edit["old"])
        if count != 1:
            fail(f'{sabotage["name"]}: "{edit["old"][:60]}" occurs {count} times in {edit["file"]}, not once')
        contents[path] = text.replace(edit["old"], edit["new"])
    return contents


def rebuild(build_dir, touched, clean_first=False):
    clean_first = clean_first or any(p.suffix not in (".cpp", ".cc", ".cxx") for p in touched)
    if not clean_first:
        for p in touched:
            for obj in build_dir.rglob(p.name + ".o"):
                obj.unlink()
            for obj in build_dir.rglob(p.name + ".obj"):
                obj.unlink()
    cmd = ["cmake", "--build", str(build_dir)] + (["--clean-first"] if clean_first else [])
    return subprocess.run(cmd, capture_output=True, text=True)


def output_hashes(build_dir):
    """{path relative to BUILD_DIR: SHA-256} of the files in BUILD_DIR/bin and BUILD_DIR/lib."""
    hashes = {}
    for sub in ("bin", "lib"):
        for path in sorted((build_dir / sub).rglob("*")):
            if path.is_file():
                hashes[str(path.relative_to(build_dir))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return hashes


def differing_from_head(root, files):
    """The files whose content differs from HEAD, read from the disk (git diff and git status trust the stat cache)."""
    differ = []
    for f in files:
        head = git(root, "rev-parse", "--verify", "--quiet", f"HEAD:{f}", check=False).stdout.strip()
        disk = git(root, "hash-object", "--", f, check=False).stdout.strip()
        if not head or head != disk:
            differ.append(f)
    return differ


def build_tail(build):
    return "\n  ".join((build.stdout + build.stderr).strip().splitlines()[-15:])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sabotages", type=Path, help="JSON file with the tests and the sabotages")
    parser.add_argument("--build-dir", type=Path, default=Path("build"), help="relative to the repository root")
    parser.add_argument("--only", nargs="+", default=[], help="run only the sabotages whose name starts with these")
    parser.add_argument("--require-detection", action="store_true",
                        help="exit 2 if a sabotage is missed in a configuration")
    parser.add_argument("--markdown", type=Path, help="also write the result table to this file")
    parser.add_argument("--jobs", type=int, default=len(LABELS), help="configurations run in parallel")
    parser.add_argument("--timeout", type=int, default=1800, help="seconds per test binary and configuration")
    args = parser.parse_args()

    root = Path(git(Path.cwd(), "rev-parse", "--show-toplevel").stdout.strip()).resolve()
    build_dir = (root / args.build_dir).resolve()
    bin_dir = build_dir / "bin"
    spec = json.loads(args.sabotages.read_text(encoding="utf-8"))
    default_tests = spec.get("tests", [])
    sabotages = [s for s in spec.get("sabotages", [])
                 if not args.only or any(s["name"].startswith(o) for o in args.only)]
    if not sabotages:
        fail("no sabotage to run")
    for s in sabotages:  # before any file is read or written
        for edit in s.get("edits", []):
            inside_root(root, edit["file"])
    for s in sabotages:
        for t in s.get("tests", default_tests):
            if not (bin_dir / t).is_file():
                fail(f'{s["name"]}: test binary {bin_dir / t} not found (build first)')
        if not s.get("tests", default_tests):
            fail(f'{s["name"]}: no test to run')

    touched_all = sorted({edit["file"] for s in sabotages for edit in s.get("edits", [])})
    dirty = differing_from_head(root, touched_all)
    if dirty:
        fail("files to sabotage that differ from HEAD (local changes, or not in HEAD): " + ", ".join(dirty))
    for s in sabotages:  # every "old" must be found before anything is touched
        apply_edits(root, s)

    nofont_dir = Path(tempfile.mkdtemp(prefix="qiw-mutation-nofonts."))
    try:
        configs = configurations(nofont_dir)
    except RuntimeError as e:
        shutil.rmtree(nofont_dir, ignore_errors=True)
        fail(f"no environment without fonts: {e}")
    labels = list(LABELS)

    rows = []
    missed = False
    clean = {}          # output_hashes() of the clean build
    unclean = None      # why the build after a restore is not the clean build

    def run_one(name, sabotage):
        nonlocal unclean
        tests = sabotage.get("tests", default_tests)
        contents = apply_edits(root, sabotage)
        originals = {path: path.read_bytes() for path in contents}
        try:
            for path, text in contents.items():
                path.write_text(text, encoding="utf-8")
            build = rebuild(build_dir, list(contents), clean_first=not contents)
            if build.returncode != 0:
                print(f"{name}: BUILD FAILED\n  " + build_tail(build), flush=True)
                return None
            if not contents:
                clean.update(output_hashes(build_dir))
            return run_all(bin_dir, tests, configs, args.jobs, args.timeout)
        finally:
            if contents:
                for path, data in originals.items():
                    path.write_bytes(data)
                # Without this build an object of the sabotage could stay for the next one (#197)
                build = rebuild(build_dir, list(contents))
                if build.returncode != 0:
                    unclean = f"after {name}, the restored sources do not compile:\n  " + build_tail(build)
                else:
                    now = output_hashes(build_dir)
                    differ = sorted(f for f in clean if now.get(f) != clean[f])
                    if differ:
                        unclean = (f"after {name} and the restore, the build does not match the clean build: "
                                   + ", ".join(differ))

    try:
        baseline = run_one("baseline", {"name": "baseline", "edits": [], "tests": sorted(
            {t for s in sabotages for t in s.get("tests", default_tests)})})
        if baseline is None or not all(r.ok() for r in baseline):
            names = sorted(set().union(*[r.names for r in baseline])) if baseline else []
            fail("the baseline (no sabotage) fails: " + (", ".join(names) or "build failed"))
        if not clean:
            fail(f"no file in {build_dir / 'bin'} or {build_dir / 'lib'} to check the builds against")
        print("baseline: " + " ".join("0" for _ in configs), flush=True)
        for s in sabotages:
            result = run_one(s["name"], s)
            if result is None:
                rows.append((s["name"], None, "BUILD FAILED"))
                missed = True
                if unclean:
                    break
                continue
            counts = [r.fails for r in result]
            names = ", ".join(sorted(set().union(*[r.names for r in result])))
            missed = missed or any(c == 0 for c in counts)
            rows.append((s["name"], counts, names))
            print(f'{s["name"]}: {" ".join(str(c) for c in counts)}  | {names}', flush=True)
            if unclean:
                break
    finally:
        shutil.rmtree(nofont_dir, ignore_errors=True)
        not_restored = differing_from_head(root, touched_all)

    if args.markdown:
        lines = ["| Sabotage | " + " | ".join(labels) + " | Caught by |",
                 "|---|" + "---|" * len(labels) + "---|"]
        for name, counts, names in rows:
            cells = [str(c) for c in counts] if counts else ["–"] * len(labels)
            lines.append(f"| {name} | " + " | ".join(cells) + f" | {names} |")
        args.markdown.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print("configurations: " + ", ".join(labels))
    if not_restored:
        print("mutation_check: the sources differ from HEAD after the restore: " + ", ".join(not_restored),
              file=sys.stderr)
        return 3
    if unclean:
        print(f"mutation_check: {unclean}\nThe run stopped: no further sabotage ran on this build. Rebuild with "
              "--clean-first.", file=sys.stderr)
        return 4
    print("sources restored (same content as HEAD). Rule 8: rebuild with --clean-first and run the full ctest.")
    if args.require_detection and missed:
        print("mutation_check: at least one sabotage was missed in a configuration (or did not compile)",
              file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
