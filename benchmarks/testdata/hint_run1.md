<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->
<!-- Example input of benchmarks/test_merge_runs.py (shortened bench output, not a measurement) -->

<!-- bench_MinimumSizeHint results: begin -->

#### Environment

| Item | Value |
|---|---|
| CPU | Example CPU (20 logical CPUs, x86_64) |

#### First minimumSizeHint() of a new widget (targets on the median: default ≤ 2 ms, worst case ≤ 16.7 ms)

| Case | Minimum size | Trials | min (ms) | median (ms) | p95 (ms) | Target |
|---|---|---|---|---|---|---|
| IndustrialKnob, default | 80 | 30 | 1.512 | 1.640 | 2.900 | PASS |
| IndustrialKnob, worst case: 0 to 1e+11, precision 3, 20 major ticks | 82 | 30 | 80.100 | 92.800 | 120.400 | FAIL |

<!-- bench_MinimumSizeHint results: end -->
