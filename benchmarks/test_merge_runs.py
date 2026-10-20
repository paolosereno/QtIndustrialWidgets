#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Tests of benchmarks/merge_runs.py on the example runs in benchmarks/testdata (Python standard library only).

Run: python3 -m unittest discover -s benchmarks -p test_merge_runs.py
"""

import os
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, 'merge_runs.py')
DATA = os.path.join(HERE, 'testdata')


def merge(*names):
    result = subprocess.run([sys.executable, SCRIPT] + [os.path.join(DATA, n) for n in names],
                            capture_output=True, text=True, encoding='utf-8')
    return result.returncode, result.stdout, result.stderr


def row(text, first_cell):
    """The cells of the Markdown table row whose first cell is first_cell."""
    for line in text.splitlines():
        cells = [c.strip() for c in line.strip().strip('|').split('|')]
        if line.startswith('|') and cells[0] == first_cell:
            return cells
    raise AssertionError(f'no row starting with {first_cell!r}')


class MergeRunsTest(unittest.TestCase):
    def test_strip_chart_three_runs(self):
        code, out, err = merge('strip_run1.md', 'strip_run2.md', 'strip_run3.md')
        self.assertEqual(code, 0, err)
        self.assertIn('<!-- bench_StripChart results: begin -->', out)
        self.assertIn('from 3 runs', out)
        self.assertIn('| Median, runs 1–3 | Median, range | p95, runs 1–3 | p95, range |', out)
        # Every run is published, then the range of the runs
        self.assertEqual(row(out, '4')[4:], ['1144058', '5.80 · 5.80 · 5.80', '6.07 · 6.12 · 6.54', '6.07–6.54',
                                             '7.40 · 7.10 · 10.32', '7.10–10.32'])
        # Ranges keep the decimals of the inputs; a column without values gives no range
        self.assertEqual(row(out, 'addSample, autoscale ON')[-2:], ['0.0323 · 0.0322 · 0.0325', '0.0322–0.0325'])
        self.assertEqual(row(out, 'addUniformSamples 1M')[-2:], ['– · – · –', '–'])
        # Environment: one value when the runs agree, each run's value otherwise
        self.assertEqual(row(out, 'CPU')[1], 'Example CPU (20 logical CPUs, x86_64)')
        self.assertEqual(row(out, 'Benchmark source commit')[1], 'run 1: aaa111; run 2: aaa111; run 3: bbb222')

    def test_analysis_chart_two_runs(self):
        code, out, err = merge('analysis_run1.md', 'analysis_run2.md')
        self.assertEqual(code, 0, err)
        self.assertIn('<!-- bench_AnalysisChart results: begin -->', out)
        full = row(out, 'Full view, redraw only (SpikesAndNaN)')
        self.assertEqual(full, ['Full view, redraw only (SpikesAndNaN)', '1e+07', '4 / 4', '33.46 · 33.46',
                                '42.07 · 39.50', '39.50–42.07', '65.22 · 61.00', '61.00–65.22', 'FAIL · FAIL'])
        self.assertEqual(row(out, 'Pan, arrow keys 10 % per step, 1/100 of the record')[-1], 'PASS · FAIL')
        self.assertEqual(row(out, 'Time per call (4 calls of 10000000 samples)')[1],
                         'run 1: min 121.85 ms, median 149.02 ms; run 2: min 118.20 ms, median 140.11 ms')
        self.assertEqual(row(out, 'Pyramid')[1], '81.1 MiB (2.13 B/sample, 13.3 % of the samples)')

    def test_runs_that_drew_different_pixels_are_rejected(self):
        code, _, err = merge('strip_run1.md', 'strip_bad_px.md')
        self.assertNotEqual(code, 0)
        self.assertIn('Trace px', err)
        self.assertIn('did not measure the same work', err)

    def test_different_benchmarks_are_rejected(self):
        code, _, err = merge('strip_run1.md', 'analysis_run1.md')
        self.assertNotEqual(code, 0)
        self.assertIn('different benchmarks', err)

    def test_one_run_is_not_a_session(self):
        code, _, err = merge('strip_run1.md')
        self.assertNotEqual(code, 0)
        self.assertIn('at least two runs', err)


if __name__ == '__main__':
    unittest.main()
