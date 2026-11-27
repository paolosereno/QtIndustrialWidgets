#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Tests of tools/ci_skips.py (#274): the skips of a `ctest -V` log against an expected skips file.

Run: python3 -m unittest discover -s tools -p test_ci_skips.py

Only the Python standard library: the log is written by the test.
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "ci_skips.py"

# A ctest -V log: tst_fixture at scale 1 (#1) and at 1.25 (#2); both skip needsFonts(test font) and #2 skips
# otherSkip()
LOG = """\
UpdateCTestConfiguration  from :build/DartConfiguration.tcl
test 1
    Start 1: tst_fixture

1: Test command: build/bin/tst_fixture
1: ********* Start testing of tst_fixture *********
1: PASS   : tst_fixture::alwaysPasses()
1: SKIP   : tst_fixture::needsFonts(test font, row 1) the test font has no letters
1:    Loc: [tst_fixture.cpp(1)]
1: Totals: 1 passed, 0 failed, 1 skipped, 0 blacklisted, 0ms
1/2 Test #1: tst_fixture ......................   Passed    0.01 sec
test 2
    Start 2: tst_fixture_dpr125

2: Test command: build/bin/tst_fixture
2: PASS   : tst_fixture::alwaysPasses()
2: SKIP   : tst_fixture::needsFonts(test font, row 1) the test font has no letters
2: SKIP   : tst_fixture::otherSkip() not approved
2: Totals: 1 passed, 0 failed, 2 skipped, 0 blacklisted, 0ms
2/2 Test #2: tst_fixture_dpr125 ...............   Passed    0.01 sec

100% tests passed, 0 tests failed out of 2
"""


def entry(function="needsFonts", data="test font, .*", configurations=("windows ci 1", "windows ci 1.25")):
    return {"test": "tst_fixture", "function": function, "data": data, "configurations": list(configurations),
            "reason": "the test font has no letters"}


class CiSkipsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="qiw-test-ci-skips."))
        self.log = self.tmp / "ctest.log"
        self.log.write_text(LOG, encoding="utf-8")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_check(self, *entries, platform="windows ci", log=None):
        skips = self.tmp / "expected_skips.json"
        skips.write_text(json.dumps({"skips": list(entries)}), encoding="utf-8")
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
        return subprocess.run([sys.executable, str(SCRIPT), str(log or self.log), "--platform", platform,
                               "--expected-skips", str(skips)], env=env, capture_output=True, text=True)

    def test_listed_skips_pass(self):
        r = self.run_check(entry(), entry("otherSkip", "", ["windows ci 1.25"]))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("2 tests, 3 expected skips, 0 not listed", r.stdout)

    def test_skip_not_listed_fails(self):
        r = self.run_check(entry())
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn("[windows ci 1.25] tst_fixture_dpr125: SKIP   : tst_fixture::otherSkip()", r.stdout)

    def test_scale_not_listed_fails(self):
        # needsFonts is listed at scale 1 only: the skip at 1.25 (tst_fixture_dpr125) is not expected
        r = self.run_check(entry(configurations=["windows ci 1"]), entry("otherSkip", "", ["windows ci 1.25"]))
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn("[windows ci 1.25] tst_fixture_dpr125: SKIP   : tst_fixture::needsFonts(test font, row 1)",
                      r.stdout)

    def test_local_configuration_does_not_cover_ci(self):
        # An entry for the local configurations of run_configs.py does not cover the CI job
        r = self.run_check(entry(configurations=["fonts 1", "fonts 1.25"]), entry("otherSkip", "", ["windows ci 1.25"]))
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)

    def test_data_tag_matched_in_full(self):
        r = self.run_check(entry(data="test font"), entry("otherSkip", "", ["windows ci 1.25"]))
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)

    def test_log_without_tests_fails(self):
        empty = self.tmp / "empty.log"
        empty.write_text("No tests were found!!!\n", encoding="utf-8")
        r = self.run_check(entry(), log=empty)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)

    def test_unknown_platform_is_refused(self):
        r = self.run_check(entry(), platform="amiga ci")
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main()
