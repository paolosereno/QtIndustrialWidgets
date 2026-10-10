<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
SPDX-License-Identifier: MIT
-->

# StripChart Performance Benchmarks

This page is the single source of performance figures for `QtIndustrialWidgets::StripChart`.
Every figure on it comes from **one run** of `bench_StripChart` (benchmark source commit `e5595b2`)
on the machine described below. README, `docs/strip_chart.md` and the CHANGELOG quote this page or link to it.

All measurements render offscreen into a `QPixmap` with Qt's raster engine: they are **CPU-only** and do not
include window-system composition or vsync.

---

## Environment

Printed by the benchmark at start-up:

| Item | Value |
|---|---|
| CPU | 13th Gen Intel(R) Core(TM) i5-13600KF (20 logical CPUs, x86_64) |
| OS | Ubuntu 24.04.1 LTS |
| Kernel | linux 6.18.40.1-microsoft-standard-WSL2 |
| Compiler | GCC 13.3.0 |
| Build | NDEBUG defined, __OPTIMIZE__ defined |
| Qt | 6.4.2 (compiled against 6.4.2) |
| QPA platform | offscreen |
| Render target | QPixmap via QWidget::render() (CPU raster) |
| Device pixel ratio | 1 |
| Widget size | 1974 × 1080 px |
| Plot width W_dev | 1920 device px |
| Library version | 2.1.1 |
| Benchmark source commit | e5595b226812abda17184b6136c3d7d9a5d911c7 |
| Heap allocation counting | malloc/calloc/realloc interposed (glibc) |

Not printed by the benchmark: the compiler flags come from the CMake `Release` configuration
(`-O3 -DNDEBUG -std=c++17 -Wall -Wextra -Wpedantic -fPIC`). The machine runs Ubuntu under WSL2 on a Windows host.

---

## Commands

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_DOCS=OFF \
      -DBUILD_DESIGNER_PLUGIN=OFF -DBUILD_BENCHMARKS=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ./build/bin/bench_StripChart -o bench_log.txt,txt > bench_results.md
```

`bench_results.md` contains the environment and results tables below verbatim; progress lines go to stderr.

---

## Method

- **Widget**: 1974 × 1080 px at DPR 1, plot width `W_dev` = 1920 device px, `DecimationMode::Auto`, default
  channel pen width 1.8 px, autoscale off unless stated. Time-mode cases use a 10 s window filled with `N` uniformly
  spaced samples per channel; the ring-buffer capacity is `N`.
- **Paint at steady state**: data inserted once, then 5 warm-up renders and 30 measured renders; min, median and
  p95 (nearest rank) of the 30 render times.
- **Paint with streaming**: before every render, 1/60 s of acquisition at the case's sample rate (`N / 10 s`) is
  appended to every channel with `addUniformSamples()`, so the ring buffer evicts as much as it receives. 5 warm-up
  and 30 measured frames; render time and append time are reported separately.
- **Heap allocations**: counted in the streaming case by interposing glibc `malloc` / `calloc` / `realloc` in the
  benchmark executable. The count is process-wide: it includes `QPainter`, text and pixmap allocations, not only the
  library's own.
- **First paint after resize**: 10 independent trials; each builds a fresh chart, renders at 1000 × 800, resizes to
  1974 × 1080 and times the next render (cold path: decimation streams are rebuilt).
- **Insertion**: real call counts, never extrapolated; 10 trials each. The `addSample` cases run against a ring
  buffer that already holds 1M samples (capacity 1M, so every call evicts one sample).
- **Signals**: `SmoothSine` (sine), `BroadbandNoise` (sine plus noise and an alternating ±3 offset, so every pixel
  column has a min/max envelope), `SpikesAndNaN` (sine with a one-sample spike every 10 000 samples and a 5-sample NaN
  gap every 25 000 samples).

**Run-to-run variation.** On this machine some cases vary noticeably between runs, most visibly the 4-channel cases
and the 1 ch × 4M cases, whose min and median can also differ widely within one run. Read min and p95 together with
the median, and re-run before drawing conclusions from small differences.

---

## Results

### Paint at steady state (5 warm-up + 30 measured renders, ms)

| Channels | Samples / ch | Signal | X axis | Min | Median | p95 |
|---:|---:|---|---|---:|---:|---:|
| 1 | 10k | SmoothSine | Time | 0.67 | 0.71 | 0.82 |
| 1 | 10k | SmoothSine | SampleIndex | 0.68 | 0.72 | 0.75 |
| 1 | 10k | BroadbandNoise | Time | 1.70 | 1.75 | 1.89 |
| 1 | 10k | BroadbandNoise | SampleIndex | 1.51 | 1.57 | 1.64 |
| 1 | 10k | SpikesAndNaN | Time | 0.67 | 0.73 | 0.83 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 0.66 | 0.70 | 0.80 |
| 1 | 100k | SmoothSine | Time | 0.79 | 0.83 | 0.96 |
| 1 | 100k | SmoothSine | SampleIndex | 0.77 | 0.84 | 0.98 |
| 1 | 100k | BroadbandNoise | Time | 2.09 | 2.18 | 2.35 |
| 1 | 100k | BroadbandNoise | SampleIndex | 2.06 | 2.16 | 2.33 |
| 1 | 100k | SpikesAndNaN | Time | 0.82 | 0.87 | 0.95 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 0.82 | 0.85 | 0.97 |
| 1 | 1M | SmoothSine | Time | 1.75 | 1.93 | 2.19 |
| 1 | 1M | SmoothSine | SampleIndex | 1.67 | 1.94 | 2.15 |
| 1 | 1M | BroadbandNoise | Time | 2.81 | 2.95 | 3.42 |
| 1 | 1M | BroadbandNoise | SampleIndex | 2.90 | 3.53 | 5.25 |
| 1 | 1M | SpikesAndNaN | Time | 1.93 | 2.23 | 2.98 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 2.05 | 2.14 | 2.59 |
| 1 | 4M | SmoothSine | Time | 4.31 | 6.01 | 9.23 |
| 1 | 4M | SmoothSine | SampleIndex | 5.07 | 6.48 | 8.50 |
| 1 | 4M | BroadbandNoise | Time | 5.41 | 7.34 | 9.57 |
| 1 | 4M | BroadbandNoise | SampleIndex | 4.34 | 7.01 | 9.72 |
| 1 | 4M | SpikesAndNaN | Time | 5.31 | 7.56 | 9.57 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 4.68 | 5.02 | 10.71 |
| 4 | 10k | SmoothSine | Time | 1.04 | 1.12 | 1.25 |
| 4 | 10k | SmoothSine | SampleIndex | 1.03 | 1.08 | 1.22 |
| 4 | 10k | BroadbandNoise | Time | 5.19 | 5.43 | 6.58 |
| 4 | 10k | BroadbandNoise | SampleIndex | 4.50 | 4.81 | 5.77 |
| 4 | 10k | SpikesAndNaN | Time | 1.08 | 1.11 | 1.21 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 0.96 | 0.99 | 1.04 |
| 4 | 100k | SmoothSine | Time | 1.45 | 1.50 | 1.73 |
| 4 | 100k | SmoothSine | SampleIndex | 1.46 | 1.53 | 1.77 |
| 4 | 100k | BroadbandNoise | Time | 6.60 | 7.23 | 11.27 |
| 4 | 100k | BroadbandNoise | SampleIndex | 6.69 | 11.61 | 19.48 |
| 4 | 100k | SpikesAndNaN | Time | 1.62 | 1.73 | 2.17 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 1.65 | 1.73 | 1.85 |
| 4 | 1M | SmoothSine | Time | 7.28 | 9.81 | 10.58 |
| 4 | 1M | SmoothSine | SampleIndex | 4.86 | 8.48 | 11.00 |
| 4 | 1M | BroadbandNoise | Time | 22.57 | 23.59 | 24.29 |
| 4 | 1M | BroadbandNoise | SampleIndex | 14.47 | 23.21 | 24.36 |
| 4 | 1M | SpikesAndNaN | Time | 5.90 | 10.02 | 13.59 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 5.98 | 6.60 | 8.98 |
| 4 | 4M | SmoothSine | Time | 15.30 | 32.59 | 38.87 |
| 4 | 4M | SmoothSine | SampleIndex | 14.41 | 31.08 | 39.00 |
| 4 | 4M | BroadbandNoise | Time | 14.49 | 25.24 | 38.47 |
| 4 | 4M | BroadbandNoise | SampleIndex | 15.56 | 35.27 | 38.44 |
| 4 | 4M | SpikesAndNaN | Time | 25.58 | 38.27 | 48.69 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 17.48 | 37.96 | 42.50 |

### Paint with streaming (1/60 s of data appended to every channel before each render; 5 warm-up + 30 measured frames, ms)

| Channels | Samples / ch | Signal | X axis | New samples / ch / frame | Render min | Render median | Render p95 | Append median (all ch) |
|---:|---:|---|---|---:|---:|---:|---:|---:|
| 1 | 10k | SmoothSine | Time | 17 | 0.63 | 0.71 | 0.92 | 0.00 |
| 1 | 10k | SmoothSine | SampleIndex | 17 | 0.64 | 0.69 | 0.75 | 0.00 |
| 1 | 10k | BroadbandNoise | Time | 17 | 1.71 | 1.80 | 2.46 | 0.00 |
| 1 | 10k | BroadbandNoise | SampleIndex | 17 | 1.52 | 1.61 | 1.89 | 0.00 |
| 1 | 10k | SpikesAndNaN | Time | 17 | 0.68 | 0.72 | 0.83 | 0.00 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 17 | 0.66 | 0.71 | 0.96 | 0.00 |
| 1 | 100k | SmoothSine | Time | 167 | 0.83 | 0.91 | 1.29 | 0.00 |
| 1 | 100k | SmoothSine | SampleIndex | 167 | 0.83 | 0.87 | 1.02 | 0.00 |
| 1 | 100k | BroadbandNoise | Time | 167 | 2.11 | 2.22 | 2.44 | 0.00 |
| 1 | 100k | BroadbandNoise | SampleIndex | 167 | 2.11 | 2.23 | 2.38 | 0.00 |
| 1 | 100k | SpikesAndNaN | Time | 167 | 0.84 | 0.90 | 0.99 | 0.00 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 167 | 0.84 | 0.89 | 1.08 | 0.00 |
| 1 | 1M | SmoothSine | Time | 1667 | 1.68 | 1.89 | 2.33 | 0.01 |
| 1 | 1M | SmoothSine | SampleIndex | 1667 | 1.78 | 2.17 | 5.79 | 0.01 |
| 1 | 1M | BroadbandNoise | Time | 1667 | 2.85 | 3.01 | 3.58 | 0.01 |
| 1 | 1M | BroadbandNoise | SampleIndex | 1667 | 2.88 | 3.15 | 6.25 | 0.01 |
| 1 | 1M | SpikesAndNaN | Time | 1667 | 1.89 | 2.11 | 2.67 | 0.01 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 1667 | 2.07 | 2.52 | 5.91 | 0.01 |
| 1 | 4M | SmoothSine | Time | 6667 | 4.43 | 6.55 | 8.44 | 0.12 |
| 1 | 4M | SmoothSine | SampleIndex | 6667 | 6.12 | 9.91 | 47.06 | 0.12 |
| 1 | 4M | BroadbandNoise | Time | 6667 | 4.29 | 4.60 | 5.94 | 0.03 |
| 1 | 4M | BroadbandNoise | SampleIndex | 6667 | 5.76 | 10.45 | 46.07 | 0.12 |
| 1 | 4M | SpikesAndNaN | Time | 6667 | 4.56 | 4.89 | 7.15 | 0.03 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 6667 | 4.78 | 10.47 | 40.96 | 0.05 |
| 4 | 10k | SmoothSine | Time | 17 | 1.00 | 1.08 | 1.24 | 0.00 |
| 4 | 10k | SmoothSine | SampleIndex | 17 | 0.96 | 1.03 | 1.17 | 0.00 |
| 4 | 10k | BroadbandNoise | Time | 17 | 5.11 | 5.30 | 5.96 | 0.00 |
| 4 | 10k | BroadbandNoise | SampleIndex | 17 | 4.42 | 4.61 | 5.06 | 0.00 |
| 4 | 10k | SpikesAndNaN | Time | 17 | 1.02 | 1.05 | 1.24 | 0.00 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 17 | 0.98 | 1.08 | 1.17 | 0.00 |
| 4 | 100k | SmoothSine | Time | 167 | 1.48 | 1.61 | 1.89 | 0.00 |
| 4 | 100k | SmoothSine | SampleIndex | 167 | 1.44 | 1.53 | 1.89 | 0.00 |
| 4 | 100k | BroadbandNoise | Time | 167 | 6.97 | 8.01 | 10.29 | 0.01 |
| 4 | 100k | BroadbandNoise | SampleIndex | 167 | 6.92 | 7.64 | 10.15 | 0.01 |
| 4 | 100k | SpikesAndNaN | Time | 167 | 1.57 | 1.68 | 2.03 | 0.00 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 167 | 1.60 | 1.73 | 2.08 | 0.00 |
| 4 | 1M | SmoothSine | Time | 1667 | 4.91 | 6.42 | 10.17 | 0.04 |
| 4 | 1M | SmoothSine | SampleIndex | 1667 | 5.30 | 8.68 | 43.96 | 0.12 |
| 4 | 1M | BroadbandNoise | Time | 1667 | 8.92 | 12.96 | 18.03 | 0.04 |
| 4 | 1M | BroadbandNoise | SampleIndex | 1667 | 9.74 | 23.59 | 62.22 | 0.12 |
| 4 | 1M | SpikesAndNaN | Time | 1667 | 5.62 | 6.05 | 11.75 | 0.03 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 1667 | 6.17 | 12.90 | 48.95 | 0.12 |
| 4 | 4M | SmoothSine | Time | 6667 | 20.30 | 32.34 | 35.51 | 0.48 |
| 4 | 4M | SmoothSine | SampleIndex | 6667 | 26.87 | 34.80 | 186.48 | 0.48 |
| 4 | 4M | BroadbandNoise | Time | 6667 | 17.94 | 37.23 | 39.48 | 0.49 |
| 4 | 4M | BroadbandNoise | SampleIndex | 6667 | 19.53 | 38.53 | 191.87 | 0.50 |
| 4 | 4M | SpikesAndNaN | Time | 6667 | 17.27 | 27.34 | 38.47 | 0.41 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 6667 | 16.75 | 41.68 | 189.85 | 0.48 |

### Heap allocations in the streaming case (per measured frame; median / max over 30 frames)

| Channels | Samples / ch | Signal | X axis | Allocations per render | KiB allocated per render | Allocations per append (all ch) |
|---:|---:|---|---|---:|---:|---:|
| 1 | 10k | SmoothSine | Time | 125 / 131 | 138.1 / 150.2 | 0 / 0 |
| 1 | 10k | SmoothSine | SampleIndex | 125 / 130 | 138.1 / 148.1 | 0 / 0 |
| 1 | 10k | BroadbandNoise | Time | 126 / 132 | 266.1 / 278.2 | 0 / 0 |
| 1 | 10k | BroadbandNoise | SampleIndex | 128 / 132 | 269.1 / 278.2 | 0 / 0 |
| 1 | 10k | SpikesAndNaN | Time | 127 / 132 | 142.1 / 152.2 | 0 / 0 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 125 / 131 | 138.1 / 150.2 | 0 / 0 |
| 1 | 100k | SmoothSine | Time | 125 / 130 | 138.1 / 148.1 | 0 / 0 |
| 1 | 100k | SmoothSine | SampleIndex | 127 / 131 | 142.1 / 150.2 | 0 / 0 |
| 1 | 100k | BroadbandNoise | Time | 128 / 132 | 269.1 / 278.2 | 0 / 0 |
| 1 | 100k | BroadbandNoise | SampleIndex | 126 / 130 | 266.1 / 274.1 | 0 / 0 |
| 1 | 100k | SpikesAndNaN | Time | 130 / 138 | 168.0 / 184.2 | 0 / 0 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 133 / 137 | 174.1 / 182.2 | 0 / 0 |
| 1 | 1M | SmoothSine | Time | 126 / 132 | 265.0 / 278.2 | 0 / 0 |
| 1 | 1M | SmoothSine | SampleIndex | 126 / 132 | 266.1 / 278.2 | 0 / 0 |
| 1 | 1M | BroadbandNoise | Time | 128 / 133 | 269.1 / 280.2 | 0 / 0 |
| 1 | 1M | BroadbandNoise | SampleIndex | 125 / 132 | 264.0 / 278.2 | 0 / 0 |
| 1 | 1M | SpikesAndNaN | Time | 204 / 209 | 1324.1 / 1334.2 | 0 / 0 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 203 / 209 | 1321.0 / 1334.2 | 0 / 0 |
| 1 | 4M | SmoothSine | Time | 124 / 128 | 262.0 / 270.1 | 0 / 0 |
| 1 | 4M | SmoothSine | SampleIndex | 123 / 130 | 260.0 / 274.1 | 0 / 0 |
| 1 | 4M | BroadbandNoise | Time | 124 / 132 | 262.0 / 278.2 | 0 / 0 |
| 1 | 4M | BroadbandNoise | SampleIndex | 122 / 128 | 258.0 / 270.1 | 0 / 0 |
| 1 | 4M | SpikesAndNaN | Time | 442 / 448 | 5158.0 / 5172.1 | 0 / 0 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 440 / 445 | 5155.0 / 5166.1 | 0 / 0 |
| 4 | 10k | SmoothSine | Time | 209 / 218 | 480.8 / 498.9 | 0 / 0 |
| 4 | 10k | SmoothSine | SampleIndex | 212 / 218 | 485.8 / 498.9 | 0 / 0 |
| 4 | 10k | BroadbandNoise | Time | 215 / 222 | 996.8 / 1010.9 | 0 / 0 |
| 4 | 10k | BroadbandNoise | SampleIndex | 215 / 221 | 996.8 / 1008.9 | 0 / 0 |
| 4 | 10k | SpikesAndNaN | Time | 209 / 221 | 480.8 / 505.0 | 0 / 0 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 213 / 216 | 488.8 / 494.9 | 0 / 0 |
| 4 | 100k | SmoothSine | Time | 212 / 218 | 486.8 / 498.9 | 0 / 0 |
| 4 | 100k | SmoothSine | SampleIndex | 213 / 218 | 487.8 / 498.9 | 0 / 0 |
| 4 | 100k | BroadbandNoise | Time | 214 / 219 | 993.8 / 1004.9 | 0 / 0 |
| 4 | 100k | BroadbandNoise | SampleIndex | 214 / 221 | 993.8 / 1008.9 | 0 / 0 |
| 4 | 100k | SpikesAndNaN | Time | 235 / 242 | 612.8 / 626.9 | 0 / 0 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 237 / 242 | 615.8 / 626.9 | 0 / 0 |
| 4 | 1M | SmoothSine | Time | 214 / 222 | 993.8 / 1010.9 | 0 / 0 |
| 4 | 1M | SmoothSine | SampleIndex | 212 / 220 | 990.8 / 1006.9 | 0 / 0 |
| 4 | 1M | BroadbandNoise | Time | 213 / 219 | 992.8 / 1004.9 | 0 / 0 |
| 4 | 1M | BroadbandNoise | SampleIndex | 212 / 219 | 990.8 / 1004.9 | 0 / 0 |
| 4 | 1M | SpikesAndNaN | Time | 522 / 527 | 5217.8 / 5228.9 | 0 / 0 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 521 / 528 | 5216.8 / 5230.9 | 0 / 0 |
| 4 | 4M | SmoothSine | Time | 213 / 221 | 992.8 / 1008.9 | 0 / 0 |
| 4 | 4M | SmoothSine | SampleIndex | 212 / 217 | 990.8 / 1000.8 | 0 / 0 |
| 4 | 4M | BroadbandNoise | Time | 212 / 218 | 990.8 / 1002.9 | 0 / 0 |
| 4 | 4M | BroadbandNoise | SampleIndex | 213 / 220 | 992.8 / 1006.9 | 0 / 0 |
| 4 | 4M | SpikesAndNaN | Time | 1479 / 1484 | 20572.7 / 20582.8 | 0 / 0 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 1480 / 1483 | 20573.8 / 20580.8 | 0 / 0 |

### First paint after resize (cold path)

| Case | Work per trial | Trials | Min (ms) | Median (ms) | p95 (ms) | Median per call (µs) |
|---|---:|---:|---:|---:|---:|---:|
| 4 ch × 1M BroadbandNoise, Time; render at 1000×800, resize to 1974×1080, time the next render | 1 render | 10 | 29.46 | 62.54 | 69.13 | – |

### Insertion

| Case | Work per trial | Trials | Min (ms) | Median (ms) | p95 (ms) | Median per call (µs) |
|---|---:|---:|---:|---:|---:|---:|
| `addSample`, autoscale ON, 1M samples in buffer | 100000 calls | 10 | 3.20 | 3.26 | 3.50 | 0.0326 |
| `addSample`, autoscale OFF, 1M samples in buffer | 100000 calls | 10 | 3.00 | 3.22 | 4.11 | 0.0322 |
| `addUniformSamples`, one 1M-sample block into an empty channel | 1 call (1M samples) | 10 | 8.87 | 9.03 | 9.36 | – |
| `addDataPoint` (legacy), SampleIndex, autoscale OFF, empty channel | 100000 calls | 10 | 3.06 | 3.24 | 3.59 | 0.0324 |

---

## Targets

The three acceptance targets set for 2.1.0, evaluated on the steady-state median as originally defined; p95 and the
streaming value are given for reference. The targets are unchanged.

| Target | Measured | Result |
|---|---|---|
| 4 ch × 1M noisy ≤ **8 ms** per frame | Time: median **23.59 ms** (p95 24.29 ms); SampleIndex: median 23.21 ms (p95 24.36 ms); streaming, Time: median 12.96 ms (p95 18.03 ms) | **FAIL** |
| Paint-time growth ≤ **25 %** from 100k to 4M samples (1 ch, noisy) | Time: 2.18 → 7.34 ms = **+237 %**; SampleIndex: 2.16 → 7.01 ms = **+225 %** (steady-state medians) | **FAIL** |
| `addSample` with autoscale ON ≤ **1 µs** per call | **0.0326 µs** per call (median of 10 trials × 100000 calls, 1M samples in buffer) | **PASS** |

**Dominant cost of the failing paint targets.** A callgrind profile of 4 ch × 1M BroadbandNoise (Time, steady
state, collection limited to `StripChart::paintEvent`) attributes **93.8 %** of the instructions to
`QPainter::drawPolyline` → `QRasterPaintEngine::drawPolygon`, i.e. rasterising the decimated trace polylines inside
Qt; extracting buckets from the decimation stream is below 1 %. The hot function inside Qt has no symbol in the Qt
build used here. Profile details:
[#5 (comment)](https://github.com/paolosereno/QtIndustrialWidgets/issues/5#issuecomment-6101732505).
The growth target has not been profiled separately. Follow-up:
[#5](https://github.com/paolosereno/QtIndustrialWidgets/issues/5),
[#6](https://github.com/paolosereno/QtIndustrialWidgets/issues/6).

Other findings from this run, tracked separately:
- Streaming in SampleIndex mode shows periodic slow frames (p95 column of the streaming table):
  [#7](https://github.com/paolosereno/QtIndustrialWidgets/issues/7).
- With NaN gaps, allocations per frame grow with the number of visible gaps (`SpikesAndNaN` rows of the allocation
  table): [#8](https://github.com/paolosereno/QtIndustrialWidgets/issues/8).

---

## Baseline: 2.0.1, re-measured in the same session

Measured right after the run above, on the same machine, with the **same benchmark source** (`e5595b2`)
compiled against the library at tag `v2.0.1` (commit `f2fdcb7`) with the same compiler and `Release` flags.
2.0.1 has no `benchmarks/` directory, so the source file was copied into a `v2.0.1` worktree and built with a small
CMake project that adds the worktree with `add_subdirectory()` and links `QtIndustrialWidgets`, `Qt6::Widgets` and
`Qt6::Test`. The benchmark reports that build as `f2fdcb7 (bench_StripChart.cpp differs from this commit)`, library
version 2.0.1.

```bash
git worktree add --detach ../wt-v2.0.1 v2.0.1
cp benchmarks/bench_StripChart.cpp ../wt-v2.0.1/benchmarks/
# configure the small CMake project with -G Ninja -DCMAKE_BUILD_TYPE=Release and build it, then:
BENCH_ADDSAMPLE_CALLS=16000 BENCH_INSERT_TRIALS=1 QT_QPA_PLATFORM=offscreen ./bench_StripChart \
    -o base_log.txt,txt \
    paintSteady:1ch_1M_BroadbandNoise_Time paintSteady:4ch_1M_BroadbandNoise_Time \
    insertionThroughput_addSample_autoscaleOn > base_results.md
```

The `addSample` case uses **16000 calls in 1 trial** on 2.0.1 (about one minute) instead of 10 × 100 000,
because each call costs milliseconds on 2.0.1.

| Case | 2.0.1: min / median / p95 | 2.1.1 (this page): min / median / p95 |
|---|---|---|
| Paint, steady, 1 ch × 1M BroadbandNoise, Time | 5855.47 / **7413.47** / 7949.81 ms | 2.81 / **2.95** / 3.42 ms |
| Paint, steady, 4 ch × 1M BroadbandNoise, Time | 23221.93 / **31832.63** / 32707.65 ms | 22.57 / **23.59** / 24.29 ms |
| `addSample`, autoscale ON, 1M samples in buffer (median per call) | **3635.9857 µs** (16000 calls, 1 trial) | **0.0326 µs** (100000 calls, 10 trials) |

Earlier 2.0.1 baseline figures in this file and in the CHANGELOG, including an extrapolated `addSample` figure, were
not reproducible and have been removed. For an earlier 2.0.0 figure that came from a lighter case, see the
[note on issue #4](https://github.com/paolosereno/QtIndustrialWidgets/issues/4#issuecomment-6101475950).
