<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

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
| 1 | 100k | BroadbandNoise | Time | 437060 | 2.50 | 2.64 | 2.80 |
| 4 | 4M | SpikesAndNaN | Time | 1144999 | 5.80 | 6.07 | 7.40 |

#### Paint at steady state, 1 ch x 100k and 1 ch x 4M interleaved (rendered alternately in the same loop; 5 warm-up + 30 measured pairs, ms; growth = per-pair ratio 4M / 100k - 1)

| Channels | Signal | X axis | Trace px 100k | Trace px 4M | 100k median | 100k p95 | 4M median | 4M p95 | Growth, median of per-pair ratios (%) |
|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | BroadbandNoise | Time | 434775 | 499675 | 2.86 | 3.49 | 3.03 | 3.41 | 5.4 |
| 1 | BroadbandNoise | SampleIndex | 439032 | 504541 | 2.99 | 3.47 | 3.18 | 3.76 | 6.3 |

#### Insertion

| Case | Work per trial | Trials | Min (ms) | Median (ms) | p95 (ms) | Median per call (µs) |
|---|---:|---:|---:|---:|---:|---:|
| addSample, autoscale ON | 100000 calls | 10 | 3.10 | 3.23 | 3.40 | 0.0323 |
| addUniformSamples 1M | 1 call | 10 | 4.00 | 4.10 | 4.30 | – |

<!-- bench_StripChart results: end -->
