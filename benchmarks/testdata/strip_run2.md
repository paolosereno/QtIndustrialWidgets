<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->
<!-- Example input of benchmarks/test_merge_runs.py (shortened bench output, not a measurement) -->

<!-- bench_StripChart results: begin -->

#### Environment

| Item | Value |
|---|---|
| CPU | Example CPU (20 logical CPUs, x86_64) |
| Benchmark source commit | aaa111 |

#### Paint at steady state (5 warm-up + 30 measured renders, ms)

| Channels | Samples / ch | Signal | X axis | Trace px | Min | Median | p95 |
|---:|---:|---|---|---:|---:|---:|---:|
| 1 | 100k | BroadbandNoise | Time | 437060 | 2.50 | 2.66 | 3.10 |
| 4 | 4M | SpikesAndNaN | Time | 1144058 | 5.80 | 6.12 | 7.10 |

#### Insertion

| Case | Work per trial | Trials | Min (ms) | Median (ms) | p95 (ms) | Median per call (µs) |
|---|---:|---:|---:|---:|---:|---:|
| addSample, autoscale ON | 100000 calls | 10 | 3.10 | 3.23 | 3.40 | 0.0322 |
| addUniformSamples 1M | 1 call | 10 | 4.00 | 4.10 | 4.30 | – |

<!-- bench_StripChart results: end -->
