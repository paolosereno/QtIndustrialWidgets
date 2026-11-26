#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Tests of the expected skips of tools/run_configs.py (#226).

Run: python3 -m unittest discover -s tools -p test_run_configs.py

Needs git and fc-list (the configurations without fonts of run_configs.py): Linux.

The test binary is a Python script that prints what run_configs.py reads from a QtTest binary. In the configurations
without fonts (FONTCONFIG_FILE set) it skips needsFonts(a) and needsFonts(b); with the file "other-skip" in the
repository it also skips otherSkip() everywhere. An expected skip passes; any other skip still fails the run.
"""

import json
import os
import subprocess
import sys
import tempfile
import shutil
import stat
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "run_configs.py"
ALL = ["fonts 1", "fonts 1.25", "fonts 1.5", "fonts 2", "no fonts 1", "no fonts 1.25", "no fonts 1.5", "no fonts 2"]
NO_FONTS = ALL[4:]

FIXTURE = """\
#!{python}
import os
from pathlib import Path
repo = Path({repo!r})
skipped = 0
print("PASS   : tst_fixture::alwaysPasses()")
if os.environ.get("FONTCONFIG_FILE"):
    for tag in ("a", "b"):
        print(f"SKIP   : tst_fixture::needsFonts({{tag}}) no fonts (checked elsewhere)")
        print("   Loc: [tst_fixture.cpp(1)]")
        skipped += 1
else:
    print("PASS   : tst_fixture::needsFonts(a)")
    print("PASS   : tst_fixture::needsFonts(b)")
if (repo / "other-skip").exists():
    print("SKIP   : tst_fixture::otherSkip() not approved")
    skipped += 1
print(f"Totals: {{1 if skipped == 2 else 3}} passed, 0 failed, {{skipped}} skipped, 0 blacklisted, 0ms")
"""


def skip_entry(function="needsFonts", data=".*", configurations=None, reason="no fonts: checked elsewhere"):
    return {"test": "tst_fixture", "function": function, "data": data,
            "configurations": NO_FONTS if configurations is None else configurations, "reason": reason}


class ExpectedSkipsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="qiw-test-run-configs."))
        self.repo = self.tmp / "repo"
        (self.repo / "build" / "bin").mkdir(parents=True)
        (self.repo / "tests").mkdir()
        r = subprocess.run(["git", "init", "-q"], cwd=self.repo, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        binary = self.repo / "build" / "bin" / "tst_fixture"
        binary.write_text(FIXTURE.format(python=sys.executable, repo=str(self.repo)), encoding="utf-8")
        binary.chmod(binary.stat().st_mode | stat.S_IXUSR)
        self.markdown = self.tmp / "table.md"

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def expect(self, *entries):
        (self.repo / "tests" / "expected_skips.json").write_text(json.dumps({"skips": list(entries)}),
                                                                 encoding="utf-8")

    def run_configs(self):
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
        return subprocess.run([sys.executable, str(SCRIPT), "tst_fixture", "--markdown", str(self.markdown)],
                              cwd=self.repo, env=env, capture_output=True, text=True)

    def rows(self):
        """{configuration: (skipped, expected skips, result)} from the --markdown table."""
        rows = {}
        for line in self.markdown.read_text(encoding="utf-8").splitlines():
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if cells and cells[0] in ALL:
                rows[cells[0]] = (int(cells[3]), int(cells[4]), cells[5])
        return rows

    def test_expected_skips_pass(self):
        self.expect(skip_entry())
        r = self.run_configs()
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        rows = self.rows()
        for label in ALL:
            self.assertEqual(rows[label], (0, 2 if label in NO_FONTS else 0, "PASS"), label)

    def test_skips_without_the_file_fail(self):
        r = self.run_configs()
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        rows = self.rows()
        for label in NO_FONTS:
            self.assertEqual(rows[label], (2, 0, "FAIL"), label)
        for label in ALL[:4]:
            self.assertEqual(rows[label], (0, 0, "PASS"), label)

    def test_another_skip_still_fails(self):
        self.expect(skip_entry())
        (self.repo / "other-skip").write_text("", encoding="utf-8")
        r = self.run_configs()
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        rows = self.rows()
        for label in ALL:
            self.assertEqual(rows[label], (1, 2 if label in NO_FONTS else 0, "FAIL"), label)
        self.assertIn("otherSkip", r.stdout)

    def test_skip_in_a_configuration_not_listed_fails(self):
        self.expect(skip_entry(configurations=["no fonts 1"]))
        r = self.run_configs()
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        rows = self.rows()
        self.assertEqual(rows["no fonts 1"], (0, 2, "PASS"))
        for label in NO_FONTS[1:]:
            self.assertEqual(rows[label], (2, 0, "FAIL"), label)

    def test_data_tag_not_matched_fails(self):
        self.expect(skip_entry(data="a"))
        r = self.run_configs()
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        for label in NO_FONTS:
            self.assertEqual(self.rows()[label], (1, 1, "FAIL"), label)

    def test_invalid_file_is_refused(self):
        for entry in (skip_entry(configurations=["no fonts 3"]), skip_entry(reason=" "),
                      skip_entry(data="(")):
            with self.subTest(entry=entry):
                self.expect(entry)
                r = self.run_configs()
                self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
                self.assertIn("invalid expected skips file", r.stderr)
                self.assertNotIn("configurations passed", r.stdout)


if __name__ == "__main__":
    unittest.main()
