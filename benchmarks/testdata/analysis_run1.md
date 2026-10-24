<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->
<!-- Example input of benchmarks/test_merge_runs.py (shortened bench output, not a measurement) -->

<!-- bench_AnalysisChart results: begin -->

#### Environment

| Item | Value |
|---|---|
| CPU | Example CPU (20 logical CPUs, x86_64) |

#### Redraw after a zoom or pan step (target ≤ 16.7 ms at p95)

| Case | Samples per channel in view | Decimated channels | min (ms) | median (ms) | p95 (ms) | Target |
|---|---|---|---|---|---|---|
| Full view, redraw only (SpikesAndNaN) | 1e+07 | 4 / 4 | 33.46 | 42.07 | 65.22 | FAIL |
| Pan, arrow keys 10 % per step, 1/100 of the record | 1e+05 | 4 / 4 | 7.06 | 10.06 | 10.81 | PASS |

#### setChannelData()

| Item | Value |
|---|---|
| Time per call (4 calls of 10000000 samples) | min 121.85 ms, median 149.02 ms |
| Pyramid | 81.1 MiB (2.13 B/sample, 13.3 % of the samples) |

<!-- bench_AnalysisChart results: end -->
