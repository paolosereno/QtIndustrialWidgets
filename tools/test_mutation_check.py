#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Tests of tools/mutation_check.py on a small CMake project that each test writes and builds (#197).

Run: python3 -m unittest discover -s tools -p test_mutation_check.py

Needs git, cmake, a C++ compiler and fc-list (the configurations without fonts of run_configs.py): Linux.

The project has two sources, a.cpp and b.cpp, and one test binary that checks only a.cpp. Sabotage A (in a.cpp) is
caught; sabotage B (in b.cpp) cannot be caught. Each test runs A then B through a cmake wrapper placed first on PATH:
  - "clock back": before each build the wrapper dates the sources back to a fixed day, as when the WSL clock jumps
    back (CLAUDE.md, rule 8). A restored source then looks older than its sabotaged object (ninja compares it with the
    time of the build recorded in .ninja_log), so a build that does not remove the object keeps sabotage A, and B
    would be reported as caught because of A. The same date is in the git index and the repository compares only
    mtime and size (core.checkStat=minimal, core.trustctime=false, as the second-only stat of a git built without
    nanoseconds): git then takes the sabotaged a.cpp, of the same size, for unchanged, so git checkout does not restore
    it and git diff does not see it;
  - "skip": the wrapper does nothing on the first build after a.cpp is restored, a rebuild that leaves the binary of
    sabotage A. mutation_check.py must notice that the binary differs from the clean build and stop.
A third test starts from a build that already holds sabotage A: the baseline must be built from the clean sources.
A fourth one has a local change in b.cpp that git's stat cache does not see (and no test catches): the script must
refuse to run, since the restore would overwrite it. A fifth one names a file outside the repository: refused before
any file is read or written.
"""

import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = Path(os.environ.get("MUTATION_CHECK_SCRIPT", HERE / "mutation_check.py"))
CONFIGS = 8

SOURCES = {
    "CMakeLists.txt": """\
        cmake_minimum_required(VERSION 3.16)
        project(fixture CXX)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        add_executable(tst_fixture tst_fixture.cpp a.cpp b.cpp)
        """,
    "a.cpp": "int valueA() { return 1; }\n",
    "b.cpp": "int valueB() { return 2; }\n",
    # Prints what run_configs.py reads from a QtTest binary: FAIL! lines and the Totals line
    "tst_fixture.cpp": """\
        #include <cstdio>
        int valueA();
        int valueB();
        int main()
        {
            volatile int b = valueB(); // b.cpp is linked, but nothing checks it
            (void)b;
            const bool aOk = valueA() == 1;
            std::printf(aOk ? "PASS   : tst_fixture::checkA()\\n" : "FAIL!  : tst_fixture::checkA() valueA() != 1\\n");
            std::printf("PASS   : tst_fixture::alwaysPasses()\\n");
            std::printf("Totals: %d passed, %d failed, 0 skipped, 0 blacklisted, 0ms\\n", aOk ? 2 : 1, aOk ? 0 : 1);
            return aOk ? 0 : 1;
        }
        """,
}

PAST = 1577836800  # 2020-01-01, the date of the sources when the clock goes back

SPEC = """\
{"tests": ["tst_fixture"],
 "sabotages": [
  {"name": "A valueA returns 0", "edits": [{"file": "a.cpp", "old": "return 1;", "new": "return 0;"}]},
  {"name": "B valueB returns 3", "edits": [{"file": "b.cpp", "old": "return 2;", "new": "return 3;"}]}
 ]}
"""

# Stands for cmake on PATH; anything but "cmake --build" goes to the real cmake unchanged
WRAPPER = """\
#!{python}
import os, subprocess, sys
from pathlib import Path
REAL = {real!r}
MODE = {mode!r}
REPO = Path({repo!r})
STATE = Path({state!r})
args = sys.argv[1:]
if args[:1] != ["--build"]:
    sys.exit(subprocess.call([REAL] + args))
a_sabotaged = "return 0;" in (REPO / "a.cpp").read_text()
state = STATE.read_text() if STATE.exists() else ""
if MODE == "skip" and not a_sabotaged and state == "a sabotaged":
    STATE.write_text("skipped")
    sys.exit(0)  # the rebuild after the restore of a.cpp does nothing
if a_sabotaged:
    STATE.write_text("a sabotaged")
if MODE == "clock back":
    for source in REPO.glob("*.cpp"):
        os.utime(source, ({past}, {past}))
sys.exit(subprocess.call([REAL] + args))
"""


def run(cmd, cwd, env=None):
    return subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True)


def table_rows(markdown):
    """{sabotage name: [failures per configuration]} from the --markdown table."""
    rows = {}
    for line in markdown.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) == CONFIGS + 2 and re.fullmatch(r"\d+", cells[1]):
            rows[cells[0]] = [int(c) for c in cells[1:1 + CONFIGS]]
    return rows


class MutationCheckTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="qiw-test-mutation."))
        self.repo = self.tmp / "repo"
        self.repo.mkdir()
        for name, text in SOURCES.items():
            (self.repo / name).write_text(textwrap.dedent(text), encoding="utf-8")
        (self.repo / ".gitignore").write_text("build/\n", encoding="utf-8")
        for source in self.repo.glob("*.cpp"):
            os.utime(source, (PAST, PAST))
        git = ["git", "-c", "user.name=test", "-c", "user.email=test@example.invalid"]
        for cmd in (["git", "init", "-q"], ["git", "config", "core.checkStat", "minimal"],
                    ["git", "config", "core.trustctime", "false"], ["git", "add", "."],
                    git + ["commit", "-q", "-m", "fixture"]):
            r = run(cmd, self.repo)
            self.assertEqual(r.returncode, 0, r.stderr)
        generator = ["-G", "Ninja"] if shutil.which("ninja") else []
        # No rule that runs CMake again. Under this WSL, CLOCK_REALTIME steps 136.3 s ahead every 5 s, for 55-185 ms,
        # and then back (measured against CLOCK_MONOTONIC, system at rest). A file written in that window is dated in
        # the future, and the files written after it look older: CMakeCache.txt can then be newer than build.ninja,
        # and ninja regenerates it without end
        r = run(["cmake", "-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=Release",
                 "-DCMAKE_SUPPRESS_REGENERATION=ON"] + generator, self.repo)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        r = run(["cmake", "--build", "build"], self.repo)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.spec = self.tmp / "sabotages.json"
        self.spec.write_text(SPEC, encoding="utf-8")
        self.markdown = self.tmp / "table.md"

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def mutation_check(self, mode):
        wrapper_dir = self.tmp / "wrapper"
        wrapper_dir.mkdir()
        wrapper = wrapper_dir / "cmake"
        wrapper.write_text(WRAPPER.format(python=sys.executable, real=shutil.which("cmake"), mode=mode, past=PAST,
                                          repo=str(self.repo), state=str(self.tmp / "wrapper-state")),
                           encoding="utf-8")
        wrapper.chmod(wrapper.stat().st_mode | stat.S_IXUSR)
        env = dict(os.environ, PATH=f"{wrapper_dir}{os.pathsep}{os.environ['PATH']}", PYTHONDONTWRITEBYTECODE="1")
        return run([sys.executable, str(SCRIPT), str(self.spec), "--markdown", str(self.markdown)], self.repo, env)

    def assertSourcesClean(self):
        # The content, not git status: the stat cache of the fixture does not see a change of the same size
        for name, text in SOURCES.items():
            self.assertEqual((self.repo / name).read_text(encoding="utf-8"), textwrap.dedent(text),
                             f"{name} is not back to HEAD")

    def test_second_sabotage_is_not_caught_because_of_the_first(self):
        r = self.mutation_check("clock back")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        rows = table_rows(self.markdown.read_text(encoding="utf-8"))
        self.assertEqual(rows.get("A valueA returns 0"), [1] * CONFIGS, r.stdout)
        # B changes nothing that the test checks: caught only if the binary still holds sabotage A
        self.assertEqual(rows.get("B valueB returns 3"), [0] * CONFIGS, r.stdout)
        self.assertSourcesClean()
        # The build is left as the clean sources: the test binary passes
        out = run([str(self.repo / "build" / "bin" / "tst_fixture")], self.repo).stdout
        self.assertNotIn("FAIL!", out)

    def test_baseline_is_built_from_the_clean_sources(self):
        # A build left with sabotage A before the run (a.cpp restored, dated before its object)
        a = self.repo / "a.cpp"
        a.write_text("int valueA() { return 0; }\n", encoding="utf-8")
        for obj in (self.repo / "build").rglob("a.cpp.o"):
            obj.unlink()
        self.assertEqual(run(["cmake", "--build", "build"], self.repo).returncode, 0)
        a.write_text("int valueA() { return 1; }\n", encoding="utf-8")
        os.utime(a, (PAST, PAST))
        self.assertEqual(run(["cmake", "--build", "build"], self.repo).returncode, 0)
        out = run([str(self.repo / "build" / "bin" / "tst_fixture")], self.repo).stdout
        self.assertIn("FAIL!", out, "precondition: the build still holds sabotage A")
        r = self.mutation_check("clock back")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        rows = table_rows(self.markdown.read_text(encoding="utf-8"))
        self.assertEqual(rows.get("A valueA returns 0"), [1] * CONFIGS, r.stdout)
        self.assertEqual(rows.get("B valueB returns 3"), [0] * CONFIGS, r.stdout)

    def test_local_change_hidden_from_the_stat_cache_is_refused(self):
        b = self.repo / "b.cpp"
        local = "int valueB() {return 2;  }\n"  # same size as in HEAD, and sabotage B still applies
        b.write_text(local, encoding="utf-8")
        os.utime(b, (PAST, PAST))
        self.assertEqual(run(["git", "status", "--porcelain"], self.repo).stdout.strip(), "",
                         "precondition: git status does not see the change")
        r = self.mutation_check("clock back")
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn("files to sabotage that differ from HEAD", r.stderr)
        self.assertIn("b.cpp", r.stderr)
        self.assertNotIn("baseline:", r.stdout)
        self.assertEqual(b.read_text(encoding="utf-8"), local, "the local change was lost")

    def test_file_outside_the_repository_is_refused(self):
        outside = self.tmp / "outside.cpp"
        outside.write_text("int valueC() { return 2; }\n", encoding="utf-8")
        self.spec.write_text('{"tests": ["tst_fixture"], "sabotages": [{"name": "X outside", "edits": '
                             '[{"file": "../outside.cpp", "old": "return 2;", "new": "return 3;"}]}]}',
                             encoding="utf-8")
        r = self.mutation_check("clock back")
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('"../outside.cpp" is outside the repository', r.stderr)
        self.assertNotIn("baseline:", r.stdout)
        self.assertEqual(outside.read_text(encoding="utf-8"), "int valueC() { return 2; }\n")
        self.assertSourcesClean()

    def test_binary_left_by_a_sabotage_stops_the_run(self):
        r = self.mutation_check("skip")
        self.assertEqual((self.tmp / "wrapper-state").read_text(), "skipped", "the wrapper skipped no build")
        self.assertEqual(r.returncode, 4, r.stdout + r.stderr)
        self.assertIn("does not match the clean build", r.stderr)
        self.assertIn("tst_fixture", r.stderr)
        # B is not run on the binary of A
        self.assertNotIn("B valueB returns 3:", r.stdout)
        self.assertSourcesClean()


if __name__ == "__main__":
    unittest.main()
