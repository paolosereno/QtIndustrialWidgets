<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
SPDX-License-Identifier: MIT
-->

# StripChart Performance Benchmarks

> **Note:** Figures predate #11–#16 (rendering changes); regeneration tracked in [#18](https://github.com/paolosereno/QtIndustrialWidgets/issues/18).

> `AnalysisChart` figures are in their own part at the end of this page:
> [AnalysisChart Performance Benchmarks](#analysischart-performance-benchmarks).

This page is the single source of performance figures for `QtIndustrialWidgets::StripChart`.
The figures come from **two consecutive runs** of the same `bench_StripChart` build (benchmark source commit
`75bb7d0`) on the machine described below. Tables show the values of run 1 and, in the last column, the range of
the medians of both runs. README, `docs/strip_chart.md` and the CHANGELOG quote this page or link to it.

All measurements render offscreen into a `QPixmap` with Qt's raster engine: they are **CPU-only** and do not
include window-system composition or vsync.

---

## Environment

Printed by the benchmark at start-up (identical in both runs):

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
| Benchmark source commit | 75bb7d0ab3025439dda8e7ec1978837d3c76f0c4 |
| Heap allocation counting | malloc/calloc/realloc interposed (glibc) |

Not printed by the benchmark: the compiler flags come from the CMake `Release` configuration
(`-O3 -DNDEBUG -std=c++17 -Wall -Wextra -Wpedantic -fPIC`). The machine runs Ubuntu under WSL2 on a Windows host;
the CPU is a hybrid design (performance and efficiency cores).

---

## Commands

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_DOCS=OFF \
      -DBUILD_DESIGNER_PLUGIN=OFF -DBUILD_BENCHMARKS=ON
cmake --build build
for k in 1 2; do
  QT_QPA_PLATFORM=offscreen ./build/bin/bench_StripChart -o bench_log$k.txt,txt > bench_run$k.md
done
python3 benchmarks/merge_runs.py bench_run1.md bench_run2.md > bench_results.md
```

`bench_results.md` contains the environment and results tables below verbatim; progress lines go to stderr.
`merge_runs.py` stops if the two runs did not draw the same number of trace pixels.

---

## Method

- **Widget**: 1974 × 1080 px at DPR 1, plot width `W_dev` = 1920 device px, `DecimationMode::Auto`, default
  channel pen width 1.8 px, autoscale off unless stated. Time-mode cases use a 10 s window filled with `N` uniformly
  spaced samples per channel; the ring-buffer capacity is `N`. With `N` ≥ 10k every case draws decimated traces.
- **Signals** are functions of time `t = i · dt`, so every `N` draws the same waveform in the window:
  `SmoothSine` (sine), `BroadbandNoise` (the same sine plus per-sample noise and an alternating ±3 offset, so every
  pixel column has a min/max envelope), `SpikesAndNaN` (the sine with a one-sample spike every 100 ms and a 50 µs
  NaN dropout every 250 ms; at 10k samples a dropout covers at most one sample). At `N` = 1M the samples are
  bit-identical to the index-based generator used before.
- **Trace px**: trace pixels in the last rendered frame, counted as pixels inside the plot rectangle whose colour
  equals a channel colour (decimated traces are drawn without antialiasing). It measures how much the rasteriser
  has to draw, so growth in time can be compared with growth in drawn pixels.
- **Paint at steady state**: data inserted once, then 5 warm-up renders and 30 measured renders; min, median and
  p95 (nearest rank) of the 30 render times.
- **Paint with streaming**: before every render, 1/60 s of acquisition at the case's sample rate (`N / 10 s`) is
  appended to every channel with `addUniformSamples()`, so the ring buffer evicts as much as it receives. The appended
  blocks continue the same signal (verified: after 35 frames the streamed image is pixel-identical to a chart that
  received the same samples in one block). 5 warm-up and 30 measured frames; render and append time are reported
  separately.
- **Heap allocations**: counted in the streaming case (run 1) by interposing glibc `malloc` / `calloc` / `realloc`
  in the benchmark executable. The count is process-wide: it includes `QPainter`, text and pixmap allocations, not
  only the library's own.
- **First paint after resize**: 10 independent trials; each builds a fresh chart, renders at 1000 × 800, resizes to
  1974 × 1080 and times the next render (cold path: decimation streams are rebuilt).
- **Insertion**: real call counts, never extrapolated; 10 trials each. The `addSample` cases run against a ring
  buffer that already holds 1M samples (capacity 1M, so every call evicts one sample).

### Run-to-run variation (bimodal timings)

On this machine render times are **bimodal**: the same case runs either in a slow mode or in a mode roughly twice
as fast, within one run (compare min and median) and between runs (last column). The draw workload is the same in
both modes: trace pixel counts are identical between runs, and steady and streaming frames of the same case draw the
same pixels. In fast runs the append time, which involves no rendering, drops as well, which points to the whole
thread running faster (for example scheduling on a different core type of the hybrid CPU under WSL2, or a different
clock frequency). This explanation has not been verified. Small differences between cases on this page are
therefore not meaningful, and verdicts below are given for both runs.

---

## Results

### Paint at steady state (5 warm-up + 30 measured renders, ms)

| Channels | Samples / ch | Signal | X axis | Trace px | Min | Median | p95 | Median, 2 runs (min–max) |
|---:|---:|---|---|---:|---:|---:|---:|---:|
| 1 | 10k | SmoothSine | Time | 323992 | 1.74 | 1.89 | 2.18 | 1.89–1.95 |
| 1 | 10k | SmoothSine | SampleIndex | 325795 | 1.63 | 1.84 | 1.93 | 1.84–1.88 |
| 1 | 10k | BroadbandNoise | Time | 301104 | 1.94 | 2.06 | 2.35 | 2.06–2.08 |
| 1 | 10k | BroadbandNoise | SampleIndex | 292801 | 1.88 | 1.97 | 2.24 | 1.97–2.02 |
| 1 | 10k | SpikesAndNaN | Time | 373488 | 1.89 | 2.01 | 2.18 | 2.01–2.08 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 395533 | 1.94 | 2.07 | 2.32 | 2.02–2.07 |
| 1 | 100k | SmoothSine | Time | 324093 | 1.73 | 1.91 | 2.20 | 1.85–1.91 |
| 1 | 100k | SmoothSine | SampleIndex | 325481 | 1.68 | 1.86 | 2.00 | 1.84–1.86 |
| 1 | 100k | BroadbandNoise | Time | 437060 | 2.51 | 2.64 | 2.94 | 2.64–2.66 |
| 1 | 100k | BroadbandNoise | SampleIndex | 360977 | 2.50 | 2.58 | 2.72 | 2.58–2.65 |
| 1 | 100k | SpikesAndNaN | Time | 375778 | 1.89 | 2.04 | 2.26 | 2.04–2.06 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 397424 | 1.95 | 2.10 | 2.53 | 2.03–2.10 |
| 1 | 1M | SmoothSine | Time | 324031 | 1.70 | 1.90 | 2.18 | 1.90–1.90 |
| 1 | 1M | SmoothSine | SampleIndex | 325404 | 1.69 | 1.91 | 2.14 | 1.91–1.93 |
| 1 | 1M | BroadbandNoise | Time | 487658 | 2.80 | 2.96 | 3.15 | 2.96–3.99 |
| 1 | 1M | BroadbandNoise | SampleIndex | 371916 | 2.75 | 2.89 | 5.47 | 2.89–3.39 |
| 1 | 1M | SpikesAndNaN | Time | 367496 | 1.92 | 2.08 | 2.49 | 2.08–2.11 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 398302 | 1.94 | 2.12 | 2.37 | 2.11–2.12 |
| 1 | 4M | SmoothSine | Time | 323999 | 1.74 | 1.95 | 2.54 | 1.95–1.98 |
| 1 | 4M | SmoothSine | SampleIndex | 325391 | 1.78 | 1.92 | 2.24 | 1.84–1.92 |
| 1 | 4M | BroadbandNoise | Time | 498762 | 2.87 | 3.08 | 4.09 | 3.08–3.99 |
| 1 | 4M | BroadbandNoise | SampleIndex | 378141 | 2.85 | 3.88 | 6.64 | 3.04–3.88 |
| 1 | 4M | SpikesAndNaN | Time | 367562 | 1.89 | 2.27 | 3.45 | 2.14–2.27 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 398290 | 1.94 | 2.18 | 3.11 | 2.12–2.18 |
| 4 | 10k | SmoothSine | Time | 1091720 | 4.94 | 5.31 | 6.54 | 5.31–5.33 |
| 4 | 10k | SmoothSine | SampleIndex | 1100099 | 4.82 | 5.33 | 7.30 | 5.33–5.39 |
| 4 | 10k | BroadbandNoise | Time | 991391 | 5.96 | 6.40 | 8.59 | 6.25–6.40 |
| 4 | 10k | BroadbandNoise | SampleIndex | 976091 | 5.60 | 6.03 | 7.57 | 5.77–6.03 |
| 4 | 10k | SpikesAndNaN | Time | 1138214 | 5.89 | 6.28 | 7.92 | 6.25–6.28 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 1176758 | 5.85 | 6.22 | 7.77 | 6.22–9.24 |
| 4 | 100k | SmoothSine | Time | 1091804 | 5.13 | 5.53 | 6.89 | 5.53–6.15 |
| 4 | 100k | SmoothSine | SampleIndex | 1099809 | 5.12 | 5.51 | 8.07 | 5.27–5.51 |
| 4 | 100k | BroadbandNoise | Time | 1284392 | 8.17 | 11.92 | 19.89 | 8.52–11.92 |
| 4 | 100k | BroadbandNoise | SampleIndex | 1135608 | 7.83 | 10.88 | 20.40 | 8.58–10.88 |
| 4 | 100k | SpikesAndNaN | Time | 1143538 | 6.11 | 10.78 | 12.86 | 6.13–10.78 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 1181055 | 9.59 | 11.47 | 12.69 | 5.98–11.47 |
| 4 | 1M | SmoothSine | Time | 1091747 | 5.16 | 9.19 | 10.25 | 5.15–9.19 |
| 4 | 1M | SmoothSine | SampleIndex | 1099842 | 4.91 | 5.17 | 6.91 | 5.17–5.27 |
| 4 | 1M | BroadbandNoise | Time | 1365399 | 16.39 | 23.08 | 25.10 | 11.42–23.08 |
| 4 | 1M | BroadbandNoise | SampleIndex | 1154653 | 10.73 | 23.10 | 25.30 | 16.01–23.10 |
| 4 | 1M | SpikesAndNaN | Time | 1141303 | 7.91 | 10.99 | 12.16 | 6.37–10.99 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 1181836 | 5.65 | 6.01 | 6.35 | 6.01–10.42 |
| 4 | 4M | SmoothSine | Time | 1091775 | 5.58 | 9.48 | 10.48 | 8.95–9.48 |
| 4 | 4M | SmoothSine | SampleIndex | 1099894 | 5.18 | 8.29 | 10.04 | 5.88–8.29 |
| 4 | 4M | BroadbandNoise | Time | 1380651 | 16.95 | 22.75 | 24.21 | 22.75–23.74 |
| 4 | 4M | BroadbandNoise | SampleIndex | 1165839 | 11.84 | 23.99 | 25.10 | 23.99–24.05 |
| 4 | 4M | SpikesAndNaN | Time | 1144058 | 8.88 | 10.09 | 12.11 | 9.80–10.09 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 1181901 | 5.96 | 7.95 | 12.45 | 7.95–10.38 |

### Paint with streaming (1/60 s of data appended to every channel before each render; 5 warm-up + 30 measured frames, ms)

| Channels | Samples / ch | Signal | X axis | New samples / ch / frame | Trace px | Render min | Render median | Render p95 | Append median (all ch) | Median, 2 runs (min–max) |
|---:|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | 10k | SmoothSine | Time | 17 | 323785 | 1.75 | 1.90 | 2.10 | 0.00 | 1.87–1.90 |
| 1 | 10k | SmoothSine | SampleIndex | 17 | 325687 | 1.68 | 1.86 | 1.97 | 0.00 | 1.79–1.86 |
| 1 | 10k | BroadbandNoise | Time | 17 | 301138 | 2.00 | 2.12 | 2.34 | 0.00 | 2.05–2.12 |
| 1 | 10k | BroadbandNoise | SampleIndex | 17 | 292659 | 1.87 | 2.04 | 2.16 | 0.00 | 1.95–2.04 |
| 1 | 10k | SpikesAndNaN | Time | 17 | 372658 | 1.95 | 2.10 | 2.64 | 0.00 | 2.09–2.10 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 17 | 393724 | 1.96 | 2.11 | 2.72 | 0.00 | 2.10–2.11 |
| 1 | 100k | SmoothSine | Time | 167 | 323882 | 1.69 | 1.92 | 2.22 | 0.00 | 1.87–1.92 |
| 1 | 100k | SmoothSine | SampleIndex | 167 | 325297 | 1.59 | 1.86 | 2.09 | 0.00 | 1.86–1.89 |
| 1 | 100k | BroadbandNoise | Time | 167 | 436785 | 2.50 | 2.64 | 2.89 | 0.00 | 2.64–2.73 |
| 1 | 100k | BroadbandNoise | SampleIndex | 167 | 359342 | 2.43 | 2.58 | 3.05 | 0.00 | 2.58–2.71 |
| 1 | 100k | SpikesAndNaN | Time | 167 | 374845 | 1.85 | 2.01 | 2.15 | 0.00 | 2.01–2.08 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 167 | 394614 | 1.85 | 2.00 | 2.10 | 0.00 | 2.00–2.09 |
| 1 | 1M | SmoothSine | Time | 1667 | 323765 | 1.74 | 1.91 | 2.30 | 0.01 | 1.91–1.94 |
| 1 | 1M | SmoothSine | SampleIndex | 1667 | 325134 | 1.82 | 2.13 | 4.76 | 0.01 | 2.07–2.13 |
| 1 | 1M | BroadbandNoise | Time | 1667 | 487478 | 2.83 | 3.00 | 3.84 | 0.01 | 2.97–3.00 |
| 1 | 1M | BroadbandNoise | SampleIndex | 1667 | 368080 | 2.85 | 3.27 | 6.01 | 0.01 | 3.06–3.27 |
| 1 | 1M | SpikesAndNaN | Time | 1667 | 366869 | 2.02 | 2.11 | 2.29 | 0.01 | 2.08–2.11 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 1667 | 398536 | 2.07 | 2.87 | 10.28 | 0.01 | 2.34–2.87 |
| 1 | 4M | SmoothSine | Time | 6667 | 323731 | 1.70 | 1.90 | 3.11 | 0.03 | 1.82–1.90 |
| 1 | 4M | SmoothSine | SampleIndex | 6667 | 325055 | 2.02 | 4.06 | 39.41 | 0.12 | 3.45–4.06 |
| 1 | 4M | BroadbandNoise | Time | 6667 | 498604 | 2.90 | 3.88 | 5.33 | 0.10 | 3.80–3.88 |
| 1 | 4M | BroadbandNoise | SampleIndex | 6667 | 369437 | 3.73 | 7.36 | 43.36 | 0.12 | 7.20–7.36 |
| 1 | 4M | SpikesAndNaN | Time | 6667 | 366934 | 1.86 | 2.08 | 2.81 | 0.03 | 2.05–2.08 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 6667 | 398982 | 2.10 | 3.71 | 38.04 | 0.07 | 3.68–3.71 |
| 4 | 10k | SmoothSine | Time | 17 | 1091820 | 5.20 | 8.41 | 10.65 | 0.00 | 8.41–8.53 |
| 4 | 10k | SmoothSine | SampleIndex | 17 | 1100637 | 5.09 | 5.57 | 8.89 | 0.00 | 5.57–8.26 |
| 4 | 10k | BroadbandNoise | Time | 17 | 990855 | 6.04 | 6.42 | 8.87 | 0.00 | 6.42–12.11 |
| 4 | 10k | BroadbandNoise | SampleIndex | 17 | 977157 | 5.59 | 5.97 | 7.08 | 0.00 | 5.97–10.92 |
| 4 | 10k | SpikesAndNaN | Time | 17 | 1138621 | 5.79 | 6.36 | 8.57 | 0.00 | 6.36–9.81 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 17 | 1181589 | 5.79 | 6.46 | 7.76 | 0.00 | 6.45–6.46 |
| 4 | 100k | SmoothSine | Time | 167 | 1091847 | 5.33 | 8.35 | 9.37 | 0.01 | 5.69–8.35 |
| 4 | 100k | SmoothSine | SampleIndex | 167 | 1100394 | 5.02 | 8.37 | 11.10 | 0.01 | 5.42–8.37 |
| 4 | 100k | BroadbandNoise | Time | 167 | 1284222 | 8.47 | 9.93 | 12.84 | 0.01 | 9.93–20.83 |
| 4 | 100k | BroadbandNoise | SampleIndex | 167 | 1132627 | 8.41 | 20.52 | 22.10 | 0.02 | 20.52–20.92 |
| 4 | 100k | SpikesAndNaN | Time | 167 | 1143865 | 9.36 | 10.29 | 12.06 | 0.01 | 10.29–10.43 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 167 | 1179940 | 6.46 | 10.23 | 13.12 | 0.01 | 10.23–11.90 |
| 4 | 1M | SmoothSine | Time | 1667 | 1091652 | 7.62 | 8.21 | 10.05 | 0.12 | 8.02–8.21 |
| 4 | 1M | SmoothSine | SampleIndex | 1667 | 1100227 | 5.41 | 10.27 | 49.18 | 0.12 | 9.77–10.27 |
| 4 | 1M | BroadbandNoise | Time | 1667 | 1365550 | 16.44 | 22.84 | 24.03 | 0.12 | 22.84–23.31 |
| 4 | 1M | BroadbandNoise | SampleIndex | 1667 | 1146339 | 21.03 | 24.55 | 62.95 | 0.12 | 24.55–24.82 |
| 4 | 1M | SpikesAndNaN | Time | 1667 | 1140473 | 5.79 | 10.11 | 12.34 | 0.12 | 6.22–10.11 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 1667 | 1183007 | 9.19 | 12.81 | 51.77 | 0.12 | 12.78–12.81 |
| 4 | 4M | SmoothSine | Time | 6667 | 1091699 | 5.08 | 8.30 | 9.91 | 0.48 | 5.27–8.30 |
| 4 | 4M | SmoothSine | SampleIndex | 6667 | 1100083 | 8.40 | 10.78 | 157.64 | 0.48 | 10.72–10.78 |
| 4 | 4M | BroadbandNoise | Time | 6667 | 1380895 | 20.94 | 23.84 | 24.84 | 0.49 | 23.66–23.84 |
| 4 | 4M | BroadbandNoise | SampleIndex | 6667 | 1149178 | 21.22 | 24.87 | 171.61 | 0.49 | 24.87–24.88 |
| 4 | 4M | SpikesAndNaN | Time | 6667 | 1143351 | 5.59 | 6.56 | 12.07 | 0.25 | 6.56–9.86 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 6667 | 1183441 | 10.93 | 12.66 | 161.08 | 0.49 | 12.66–12.69 |

### Heap allocations in the streaming case (per measured frame; median / max over 30 frames)

| Channels | Samples / ch | Signal | X axis | Allocations per render | KiB allocated per render | Allocations per append (all ch) |
|---:|---:|---|---|---:|---:|---:|
| 1 | 10k | SmoothSine | Time | 127 / 131 | 141.1 / 150.1 | 0 / 0 |
| 1 | 10k | SmoothSine | SampleIndex | 125 / 130 | 138.1 / 148.1 | 0 / 0 |
| 1 | 10k | BroadbandNoise | Time | 126 / 131 | 266.1 / 276.1 | 0 / 0 |
| 1 | 10k | BroadbandNoise | SampleIndex | 127 / 130 | 268.1 / 274.1 | 0 / 0 |
| 1 | 10k | SpikesAndNaN | Time | 165 / 169 | 686.1 / 694.2 | 0 / 0 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 165 / 169 | 686.1 / 694.2 | 0 / 0 |
| 1 | 100k | SmoothSine | Time | 125 / 132 | 263.0 / 278.2 | 0 / 0 |
| 1 | 100k | SmoothSine | SampleIndex | 125 / 131 | 138.1 / 150.2 | 0 / 0 |
| 1 | 100k | BroadbandNoise | Time | 125 / 132 | 264.0 / 278.2 | 0 / 0 |
| 1 | 100k | BroadbandNoise | SampleIndex | 126 / 132 | 265.0 / 278.2 | 0 / 0 |
| 1 | 100k | SpikesAndNaN | Time | 164 / 170 | 683.1 / 696.2 | 0 / 0 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 164 / 168 | 684.1 / 692.1 | 0 / 0 |
| 1 | 1M | SmoothSine | Time | 125 / 132 | 263.0 / 278.2 | 0 / 0 |
| 1 | 1M | SmoothSine | SampleIndex | 126 / 132 | 266.1 / 278.2 | 0 / 0 |
| 1 | 1M | BroadbandNoise | Time | 125 / 132 | 264.0 / 278.2 | 0 / 0 |
| 1 | 1M | BroadbandNoise | SampleIndex | 126 / 132 | 265.0 / 278.2 | 0 / 0 |
| 1 | 1M | SpikesAndNaN | Time | 202 / 211 | 1320.0 / 1332.1 | 0 / 0 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 202 / 209 | 1319.0 / 1334.2 | 0 / 0 |
| 1 | 4M | SmoothSine | Time | 126 / 132 | 266.1 / 278.2 | 0 / 0 |
| 1 | 4M | SmoothSine | SampleIndex | 123 / 130 | 260.0 / 274.1 | 0 / 0 |
| 1 | 4M | BroadbandNoise | Time | 125 / 132 | 264.0 / 278.2 | 0 / 0 |
| 1 | 4M | BroadbandNoise | SampleIndex | 123 / 129 | 259.0 / 272.1 | 0 / 0 |
| 1 | 4M | SpikesAndNaN | Time | 203 / 209 | 1320.0 / 1334.2 | 0 / 0 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 200 / 207 | 1316.0 / 1330.1 | 0 / 0 |
| 4 | 10k | SmoothSine | Time | 210 / 216 | 482.8 / 494.9 | 0 / 0 |
| 4 | 10k | SmoothSine | SampleIndex | 211 / 218 | 484.8 / 498.9 | 0 / 0 |
| 4 | 10k | BroadbandNoise | Time | 215 / 222 | 996.8 / 1010.9 | 0 / 0 |
| 4 | 10k | BroadbandNoise | SampleIndex | 214 / 221 | 994.8 / 1008.9 | 0 / 0 |
| 4 | 10k | SpikesAndNaN | Time | 363 / 369 | 2659.8 / 2672.9 | 0 / 0 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 362 / 368 | 2658.8 / 2670.9 | 0 / 0 |
| 4 | 100k | SmoothSine | Time | 215 / 222 | 995.8 / 1010.9 | 0 / 0 |
| 4 | 100k | SmoothSine | SampleIndex | 209 / 216 | 480.8 / 494.9 | 0 / 0 |
| 4 | 100k | BroadbandNoise | Time | 214 / 220 | 994.8 / 1006.9 | 0 / 0 |
| 4 | 100k | BroadbandNoise | SampleIndex | 213 / 217 | 991.8 / 1000.8 | 0 / 0 |
| 4 | 100k | SpikesAndNaN | Time | 361 / 367 | 2655.8 / 2668.9 | 0 / 0 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 362 / 368 | 2658.8 / 2670.9 | 0 / 0 |
| 4 | 1M | SmoothSine | Time | 213 / 220 | 992.8 / 1006.9 | 0 / 0 |
| 4 | 1M | SmoothSine | SampleIndex | 212 / 218 | 990.8 / 1002.9 | 0 / 0 |
| 4 | 1M | BroadbandNoise | Time | 213 / 219 | 992.8 / 1004.9 | 0 / 0 |
| 4 | 1M | BroadbandNoise | SampleIndex | 213 / 218 | 992.8 / 1002.9 | 0 / 0 |
| 4 | 1M | SpikesAndNaN | Time | 521 / 528 | 5216.8 / 5230.9 | 0 / 0 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 520 / 525 | 5214.8 / 5224.8 | 0 / 0 |
| 4 | 4M | SmoothSine | Time | 213 / 221 | 992.8 / 1008.9 | 0 / 0 |
| 4 | 4M | SmoothSine | SampleIndex | 212 / 220 | 990.8 / 1006.9 | 0 / 0 |
| 4 | 4M | BroadbandNoise | Time | 212 / 219 | 990.8 / 1004.9 | 0 / 0 |
| 4 | 4M | BroadbandNoise | SampleIndex | 212 / 218 | 989.8 / 1002.9 | 0 / 0 |
| 4 | 4M | SpikesAndNaN | Time | 521 / 528 | 5216.8 / 5230.9 | 0 / 0 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 520 / 528 | 5214.8 / 5230.9 | 0 / 0 |

### First paint after resize (cold path)

| Case | Work per trial | Trials | Min (ms) | Median (ms) | p95 (ms) | Median per call (µs) | Median, 2 runs (min–max) |
|---|---:|---:|---:|---:|---:|---:|---:|
| 4 ch × 1M BroadbandNoise, Time; render at 1000×800, resize to 1974×1080, time the next render | 1 render | 10 | 42.05 | 66.64 | 70.86 | – | 64.78–66.64 |

### Insertion

| Case | Work per trial | Trials | Min (ms) | Median (ms) | p95 (ms) | Median per call (µs) | Median, 2 runs (min–max) |
|---|---:|---:|---:|---:|---:|---:|---:|
| `addSample`, autoscale ON, 1M samples in buffer | 100000 calls | 10 | 2.95 | 3.23 | 3.43 | 0.0323 | 3.22–3.23 |
| `addSample`, autoscale OFF, 1M samples in buffer | 100000 calls | 10 | 3.20 | 3.32 | 3.65 | 0.0332 | 3.23–3.32 |
| `addUniformSamples`, one 1M-sample block into an empty channel | 1 call (1M samples) | 10 | 8.85 | 9.03 | 9.26 | – | 6.10–9.03 |
| `addDataPoint` (legacy), SampleIndex, autoscale OFF, empty channel | 100000 calls | 10 | 3.22 | 3.25 | 3.87 | 0.0325 | 3.24–3.25 |

---

## Targets

The three acceptance targets set for 2.1.0, evaluated on the steady-state median as originally defined, for both
runs. A target is marked PASS only if it is met in both runs. The targets are unchanged.

| Target | Run 1 | Run 2 | Result |
|---|---|---|---|
| 4 ch × 1M noisy ≤ **8 ms** per frame | Time **23.08 ms**; SampleIndex 23.10 ms | Time **11.42 ms**; SampleIndex 16.01 ms | **FAIL** (both runs) |
| Paint-time growth ≤ **25 %** from 100k to 4M samples (1 ch, noisy) | Time 2.64 → 3.08 ms = **+16.7 %**; SampleIndex 2.58 → 3.88 ms = **+50.4 %** | Time 2.66 → 3.99 ms = **+50.0 %**; SampleIndex 2.65 → 3.04 ms = **+14.7 %** | **FAIL** (not met in every run) |
| `addSample` with autoscale ON ≤ **1 µs** per call | **0.0323 µs** | **0.0322 µs** | **PASS** (both runs) |

Streaming values for the first target (render median): run 1 Time 22.84 ms, SampleIndex
24.55 ms; run 2 Time 23.31 ms, SampleIndex 24.82 ms.
`addSample` figures are medians of 10 trials × 100 000 calls with 1M samples in the buffer.

**Growth target.** Over the same range the number of trace pixels grows by **+14.1 %** in Time
mode (437060 → 498762) and **+4.8 %** in SampleIndex mode
(360977 → 378141), identical in both runs. In run 1, Time mode, the growth
in time (+17 %) follows the growth in trace pixels (+14 %), so there is
no evidence of a library cost that depends on `N` beyond the image itself. The two failing values (run 1
SampleIndex, run 2 Time) come from a 4M median that fell into the slow mode of the machine
(3.88 and 3.99 ms, against 3.04 and 3.08 ms for the same
cases in the other run). Each axis passes in one run and fails in the other, so the target is marked FAIL; a final
verdict needs a measurement that is insensitive to the bimodal timings.

**Dominant cost of the 4 ch × 1M case.** A callgrind profile (Time, steady state, collection limited to
`StripChart::paintEvent`; data identical to this page at `N` = 1M) attributes **93.8 %** of the instructions to
`QPainter::drawPolyline` → `QRasterPaintEngine::drawPolygon`, i.e. rasterising the decimated trace polylines inside
Qt; extracting buckets from the decimation stream is below 1 %. The hot function inside Qt has no symbol in the Qt
build used here. Profile details:
[#5 (comment)](https://github.com/paolosereno/QtIndustrialWidgets/issues/5#issuecomment-6101732505).
Follow-up: [#5](https://github.com/paolosereno/QtIndustrialWidgets/issues/5),
[#6](https://github.com/paolosereno/QtIndustrialWidgets/issues/6).

Other findings, tracked separately:
- Streaming in SampleIndex mode shows periodic slow frames (p95 column of the streaming table):
  [#7](https://github.com/paolosereno/QtIndustrialWidgets/issues/7).
- With NaN gaps, allocations per frame grow with the number of visible gaps (`SpikesAndNaN` rows of the allocation
  table): [#8](https://github.com/paolosereno/QtIndustrialWidgets/issues/8).

---

## Baseline: 2.0.1, re-measured in the same session

Measured in the same session, on the same machine, with the benchmark at commit `e5595b2` compiled against the
library at tag `v2.0.1` (commit `f2fdcb7`), same compiler and `Release` flags. That benchmark version still used the
**previous, index-based signal generator**; at `N` = 1M, the only sample count used below, its samples are
bit-identical to those of the current generator, so the two columns draw the same data. The 2.1.1 column is run 1
of this page, with the range of the two runs in brackets.

2.0.1 has no `benchmarks/` directory, so the source file was copied into a `v2.0.1` worktree and built with a small
CMake project that adds the worktree with `add_subdirectory()` and links `QtIndustrialWidgets`, `Qt6::Widgets` and
`Qt6::Test`. The benchmark reports that build as `f2fdcb7 (bench_StripChart.cpp differs from this commit)`, library
version 2.0.1.

```bash
git worktree add --detach ../wt-v2.0.1 v2.0.1
git show e5595b2:benchmarks/bench_StripChart.cpp > ../wt-v2.0.1/benchmarks/bench_StripChart.cpp
# configure the small CMake project with -G Ninja -DCMAKE_BUILD_TYPE=Release and build it, then:
BENCH_ADDSAMPLE_CALLS=16000 BENCH_INSERT_TRIALS=1 QT_QPA_PLATFORM=offscreen ./bench_StripChart \
    -o base_log.txt,txt \
    paintSteady:1ch_1M_BroadbandNoise_Time paintSteady:4ch_1M_BroadbandNoise_Time \
    insertionThroughput_addSample_autoscaleOn > base_results.md
```

The `addSample` case uses **16000 calls in 1 trial** on 2.0.1 (about one minute) instead of 10 × 100 000,
because each call costs milliseconds on 2.0.1.

| Case | 2.0.1: min / median / p95 | 2.1.1: min / median / p95 (median range, 2 runs) |
|---|---|---|
| Paint, steady, 1 ch × 1M BroadbandNoise, Time | 5855.47 / **7413.47** / 7949.81 ms | 2.80 / **2.96** / 3.15 ms (2.96–3.99) |
| Paint, steady, 4 ch × 1M BroadbandNoise, Time | 23221.93 / **31832.63** / 32707.65 ms | 16.39 / **23.08** / 25.10 ms (11.42–23.08) |
| `addSample`, autoscale ON, 1M samples in buffer (median per call) | **3635.9857 µs** (16000 calls, 1 trial) | **0.0323 µs** (100000 calls, 10 trials; run 2: 0.0322 µs) |

Earlier 2.0.1 baseline figures in this file and in the CHANGELOG, including an extrapolated `addSample` figure, were
not reproducible and have been removed. For an earlier 2.0.0 figure that came from a lighter case, see the
[note on issue #4](https://github.com/paolosereno/QtIndustrialWidgets/issues/4#issuecomment-6101475950).

---

# AnalysisChart Performance Benchmarks

The single source of performance figures for `QtIndustrialWidgets::AnalysisChart` (#25). The figures come from
**one run** of `bench_AnalysisChart` (benchmark source commit `4232d72`) on the machine of the StripChart part
above, with `docs/analysis_chart.md`, README and CHANGELOG quoting this part.

## Target

#25: a redraw after a zoom or pan step takes **≤ 16.7 ms** for **4 channels × 10 M samples at a 1920 device px
plot**. A case passes when the **p95** of its 30 measured redraws is ≤ 16.7 ms.

**Result: not met.** 6 of the 7 cases FAIL; only panning at 1/100 of the record (100 k samples per channel in the
view) passes. The cause is below.

## Command

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_DOCS=OFF \
      -DBUILD_DESIGNER_PLUGIN=OFF -DBUILD_BENCHMARKS=ON
cmake --build build --clean-first
QT_QPA_PLATFORM=offscreen ./build/bin/bench_AnalysisChart > bench_analysis.md
```

`bench_analysis.md` contains the tables below verbatim; progress lines go to stderr.

## Method

- **Widget**: `AnalysisChart` at DPR 1, resized so that the plot is exactly 1920 device px wide (1974 × 1080 px),
  `DecimationMode::Auto`, readout band hidden, rendered into a `QPixmap` with `QWidget::render()` (CPU raster,
  offscreen, no composition or vsync).
- **Data**: 4 channels × 10 M samples every 10 µs (100 s). The signals are those of `bench_StripChart`, sample for
  sample (its reference interval is 10 µs): `SpikesAndNaN` for all zoom and pan cases, `SmoothSine` and
  `BroadbandNoise` for one more full-view case each. Over 100 s the 62.8 ms sine of these signals fills every
  pixel column of the full view with a full-height envelope.
- **Cases**: 5 warm-up renders, then 30 measured redraws; min, median and p95 (nearest rank). A measured redraw of
  a zoom or pan case is one input step delivered to the widget synchronously, then one render, timed together:
  - *Zoom in / Zoom out*: a wheel notch (`QWheelEvent`, ×1.25) at the centre of the plot, 30 steps from the full
    view in, then 30 steps back out;
  - *Pan*: arrow keys (`Qt::Key_Right` 15 times, then `Qt::Key_Left` 15 times, 10 % of the view each) at a view of
    1/10 and of 1/100 of the record.
- **setChannelData()**: wall time of each of the 4 calls; memory as the growth of the resident set size of the
  process across the 4 calls (the benchmark's own buffers freed), and the bytes of the pyramid.
- The timings of this machine are bimodal between runs (see the StripChart part); the spread between min and
  p95 within one case shows it.

## Results

<!-- bench_AnalysisChart results: begin -->

#### Environment

| Item | Value |
|---|---|
| CPU | 13th Gen Intel(R) Core(TM) i5-13600KF (20 logical CPUs, x86_64) |
| OS | Ubuntu 24.04.1 LTS |
| Kernel | linux 6.18.54.1-microsoft-standard-WSL2 |
| Compiler | GCC 13.3.0 |
| Build | NDEBUG defined, __OPTIMIZE__ defined |
| Qt | 6.4.2 (compiled against 6.4.2) |
| QPA platform | offscreen |
| Render target | QPixmap via QWidget::render() (CPU raster) |
| Device pixel ratio | 1 |
| Widget size | 1974 × 1080 px |
| Plot width W_dev | 1920 device px |
| Data | 4 channels × 10000000 samples, every 10 µs |
| Library version | 2.2.0 |
| Benchmark source commit | 4232d72e2a318594236d3c862506302a0fe65996 |

#### Redraw after a zoom or pan step (target ≤ 16.7 ms at p95)

| Case | Samples per channel in view | Decimated channels | min (ms) | median (ms) | p95 (ms) | Target |
|---|---|---|---|---|---|---|
| Full view, redraw only (SpikesAndNaN) | 1e+07 | 4 / 4 | 33.46 | 42.07 | 65.22 | FAIL |
| Zoom in, wheel x1.25 per step | 1e+07 → 1.238e+04 | 4 / 4 | 3.70 | 11.90 | 35.37 | FAIL |
| Zoom out, wheel x1.25 per step | 1.238e+04 → 1e+07 | 4 / 4 | 3.99 | 8.99 | 38.40 | FAIL |
| Pan, arrow keys 10 % per step, 1/10 of the record | 1e+06 | 4 / 4 | 12.90 | 19.10 | 20.21 | FAIL |
| Pan, arrow keys 10 % per step, 1/100 of the record | 1e+05 | 4 / 4 | 7.06 | 10.06 | 10.81 | PASS |
| Full view, redraw only (SmoothSine) | 1e+07 | 4 / 4 | 45.53 | 62.88 | 91.55 | FAIL |
| Full view, redraw only (BroadbandNoise) | 1e+07 | 4 / 4 | 40.45 | 84.56 | 104.60 | FAIL |

#### setChannelData()

| Item | Value |
|---|---|
| Time per call (4 calls of 10000000 samples) | min 121.85 ms, median 149.02 ms |
| Stored samples (8 B timestamp + 8 B value) | 610.4 MiB |
| Pyramid | 81.1 MiB (2.13 B/sample, 13.3 % of the samples) |
| Resident set growth across the 4 calls | 691.9 MiB |

<!-- bench_AnalysisChart results: end -->

## Cause of the miss (profile)

Measured during development with temporary timers around the phases of `paintEvent()` (not committed), on the
same machine and data, full view, `SpikesAndNaN`, per frame:

| Phase | Time per frame |
|---|---|
| Buckets from the pyramid (4 channels × 1920 buckets) | 7.5–10.2 ms |
| Segments from the buckets | 0.2–0.4 ms |
| Drawing the decimated polylines | 17–30 ms |
| Whole frame | 27–43 ms |

- **Drawing dominates.** Each M4 column is drawn first → min → max → last, so its vertical extent is stroked
  twice (up and back down); with an envelope in every column that is several million pixels written per frame
  for 4 channels, of which about 680 k remain visible. A separate `QPainter` test with the same pen showed the
  cost does not depend on the number of `drawPolyline()` calls (the same polylines in 4 or in 1536 calls: 7.3 ms
  each), so it follows the ink. `StripChart` draws its decimated traces the same way. #73 changed both charts to
  one vertical stroke per column; see the next section.
- **Buckets.** 4 × 1920 buckets each read up to two partial blocks of raw samples and about 2 log2(blocks) pyramid
  nodes; at 10 M samples these reads are mostly cache misses. A compact index of the first timestamp of every
  block replaced the binary search over the whole timestamp array (searches 4.4 → 3.7 ms per frame in a
  micro-benchmark).
- The zoom cases fail at p95 because their first steps are close to the full view; panning at 1/10 of the record
  (1 M samples per channel in view) fails with a p95 of 20.21 ms.

## One vertical stroke per column (#73): session of 2026-10-21

The drawing of decimated traces changed in #73: each device column is drawn as one vertical line from its minimum
to its maximum, plus the connection to the next column, instead of the polyline first → min → max → last, which
strokes the column's extent twice. This section measures that change against the drawing before it, in **one
session** on the machine of the StripChart part. The tables of the *Results* section above, and all the tables of
this file, will be regenerated by #18.

**Variants**

- *Reference*: `origin/main` at `3aaaea2` (the drawing before #73), built in its own worktree.
- *#73*: the same commit with the drawing change of #73, built in a second worktree. The change was not yet
  committed (the benchmark reports "working tree modified"); its code is the one committed as `3910dfe`, which
  differs only in comments and in the spelling of casts (`size_t(i)` became `static_cast<size_t>(i)`).

Both were built with the CMake `Release` configuration (as in *Command* above, `-DBUILD_BENCHMARKS=ON`).

**Session**: 5 rounds; each round ran `bench_AnalysisChart` once for each variant, the reference first in odd rounds
and second in even rounds, every run pinned to the logical CPUs 0–11 (the benchmark therefore reports 12 logical
CPUs). Per round and variant:

```bash
QT_QPA_PLATFORM=offscreen taskset -c 0-11 <variant build>/bin/bench_AnalysisChart > analysis_<variant>_run<r>.md
```

The 5 runs of each variant were merged with:

```bash
python3 benchmarks/merge_runs.py analysis_<variant>_run1.md … analysis_<variant>_run5.md
```

`merge_runs.py` checked that the runs of each variant measured the same work (samples in view, decimated channels).

**Full view, 4 × 10 M (`SpikesAndNaN`)**

| | Median of the 5 run medians | Run medians | Median of the 5 run p95 | Run p95 |
|---|---|---|---|---|
| Reference | 52.49 ms | 52.15–53.79 ms | 57.12 ms | 54.72–61.89 ms |
| #73 | 39.39 ms (−25.0 %) | 38.51–40.95 ms | 43.89 ms (−23.2 %) | 41.26–46.84 ms |

**Pan at 1/100 of the record (100 k samples per channel in view)**

| | Median of the 5 run medians | Run medians | Median of the 5 run p95 | Run p95 |
|---|---|---|---|---|
| Reference | 9.15 ms | 9.05–9.28 ms | 9.76 ms | 9.63–10.02 ms |
| #73 | 11.35 ms (+24.0 %) | 9.88–11.53 ms | 11.96 ms (+22.5 %) | 11.73–12.44 ms |

The pan case still passes the 16.7 ms target in all 5 runs: the highest p95 is 12.44 ms. The full-view case still
fails it.

**Why the pan case is slower**

Profile: `callgrind`, on the pan driver used for #73 (not committed), with collection limited to
`AnalysisChart::paintEvent`. It covers 5 warm-up renders and 30 pan steps.

| | Reference | #73 |
|---|---|---|
| `paintEvent` | 1 780 M instructions | 2 177 M instructions (+22 %) |
| Drawing of the decimated traces | 338 M | 735 M |

The whole difference is in the drawing of the decimated traces:
- separate lines instead of one polyline: about +141 M;
- the explicit end pixels drawn as points: 102 M;
- the coordinate transforms of those end pixels: 68 M.

With about 52 samples per column, the columns are short, so there is little doubled ink to save. The fixed cost of
each primitive dominates instead.

### Reference, 5 runs

Merged with `benchmarks/merge_runs.py` from 5 runs (run 1: analysis_base_run1.md, run 2: analysis_base_run2.md, run 3: analysis_base_run3.md, run 4: analysis_base_run4.md, run 5: analysis_base_run5.md).

#### Environment

| Item | Value |
| --- | --- |
| CPU | 13th Gen Intel(R) Core(TM) i5-13600KF (12 logical CPUs, x86_64) |
| OS | Ubuntu 24.04.1 LTS |
| Kernel | linux 6.18.54.1-microsoft-standard-WSL2 |
| Compiler | GCC 13.3.0 |
| Build | NDEBUG defined, __OPTIMIZE__ defined |
| Qt | 6.4.2 (compiled against 6.4.2) |
| QPA platform | offscreen |
| Render target | QPixmap via QWidget::render() (CPU raster) |
| Device pixel ratio | 1 |
| Widget size | 1974 × 1080 px |
| Plot width W_dev | 1920 device px |
| Data | 4 channels × 10000000 samples, every 10 µs |
| Library version | 2.3.0 |
| Benchmark source commit | 3aaaea2d40bf43c29c116aafb24d3a617de7081e |

#### Redraw after a zoom or pan step (target ≤ 16.7 ms at p95)

| Case | Samples per channel in view | Decimated channels | min (ms), runs 1–5 | median (ms), runs 1–5 | median (ms), range | p95 (ms), runs 1–5 | p95 (ms), range | Target, runs 1–5 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Full view, redraw only (SpikesAndNaN) | 1e+07 | 4 / 4 | 33.14 · 29.57 · 50.90 · 51.24 · 40.88 | 52.15 · 52.19 · 53.21 · 52.49 · 53.79 | 52.15–53.79 | 57.12 · 57.38 · 55.70 · 54.72 · 61.89 | 54.72–61.89 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Zoom in, wheel x1.25 per step | 1e+07 → 1.238e+04 | 4 / 4 | 4.31 · 3.02 · 3.34 · 6.98 · 7.05 | 12.01 · 5.44 · 12.62 · 12.48 · 12.51 | 5.44–12.62 | 34.74 · 43.16 · 41.09 · 42.72 · 42.25 | 34.74–43.16 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Zoom out, wheel x1.25 per step | 1.238e+04 → 1e+07 | 4 / 4 | 4.43 · 3.11 · 3.09 · 7.01 · 7.17 | 13.29 · 7.25 · 13.47 · 12.84 · 12.85 | 7.25–13.47 | 47.33 · 46.11 · 46.71 · 46.29 · 47.51 | 46.11–47.51 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Pan, arrow keys 10 % per step, 1/10 of the record | 1e+06 | 4 / 4 | 11.58 · 16.85 · 14.05 · 16.45 · 15.97 | 17.05 · 17.59 · 17.41 · 17.28 · 17.66 | 17.05–17.66 | 18.11 · 18.66 · 18.70 · 18.59 · 19.10 | 18.11–19.10 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Pan, arrow keys 10 % per step, 1/100 of the record | 1e+05 | 4 / 4 | 3.68 · 3.87 · 3.65 · 6.78 · 5.35 | 9.05 · 9.28 · 9.15 · 9.20 · 9.12 | 9.05–9.28 | 9.63 · 9.87 · 9.70 · 10.02 · 9.76 | 9.63–10.02 | PASS · PASS · PASS · PASS · PASS |
| Full view, redraw only (SmoothSine) | 1e+07 | 4 / 4 | 73.51 · 47.32 · 37.78 · 60.49 · 79.50 | 81.03 · 81.62 · 64.81 · 81.11 · 82.99 | 64.81–82.99 | 84.96 · 85.62 · 85.14 · 86.76 · 84.85 | 84.85–86.76 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Full view, redraw only (BroadbandNoise) | 1e+07 | 4 / 4 | 46.23 · 54.07 · 53.41 · 84.01 · 70.85 | 85.40 · 87.37 · 87.19 · 87.48 · 85.56 | 85.40–87.48 | 89.58 · 89.29 · 91.22 · 93.03 · 87.99 | 87.99–93.03 | FAIL · FAIL · FAIL · FAIL · FAIL |

#### setChannelData()

| Item | Value |
| --- | --- |
| Time per call (4 calls of 10000000 samples) | run 1: min 141.57 ms, median 177.63 ms; run 2: min 144.16 ms, median 174.43 ms; run 3: min 152.25 ms, median 174.53 ms; run 4: min 123.66 ms, median 152.89 ms; run 5: min 119.10 ms, median 171.10 ms |
| Stored samples (8 B timestamp + 8 B value) | 610.4 MiB |
| Pyramid | 81.1 MiB (2.13 B/sample, 13.3 % of the samples) |
| Resident set growth across the 4 calls | run 1: 691.9 MiB; run 2: 691.9 MiB; run 3: 691.8 MiB; run 4: 691.9 MiB; run 5: 691.8 MiB |

### #73, 5 runs

Merged with `benchmarks/merge_runs.py` from 5 runs (run 1: analysis_v2_run1.md, run 2: analysis_v2_run2.md, run 3: analysis_v2_run3.md, run 4: analysis_v2_run4.md, run 5: analysis_v2_run5.md).

#### Environment

| Item | Value |
| --- | --- |
| CPU | 13th Gen Intel(R) Core(TM) i5-13600KF (12 logical CPUs, x86_64) |
| OS | Ubuntu 24.04.1 LTS |
| Kernel | linux 6.18.54.1-microsoft-standard-WSL2 |
| Compiler | GCC 13.3.0 |
| Build | NDEBUG defined, __OPTIMIZE__ defined |
| Qt | 6.4.2 (compiled against 6.4.2) |
| QPA platform | offscreen |
| Render target | QPixmap via QWidget::render() (CPU raster) |
| Device pixel ratio | 1 |
| Widget size | 1974 × 1080 px |
| Plot width W_dev | 1920 device px |
| Data | 4 channels × 10000000 samples, every 10 µs |
| Library version | 2.3.0 |
| Benchmark source commit | 3aaaea2d40bf43c29c116aafb24d3a617de7081e (working tree modified) |

#### Redraw after a zoom or pan step (target ≤ 16.7 ms at p95)

| Case | Samples per channel in view | Decimated channels | min (ms), runs 1–5 | median (ms), runs 1–5 | median (ms), range | p95 (ms), runs 1–5 | p95 (ms), range | Target, runs 1–5 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Full view, redraw only (SpikesAndNaN) | 1e+07 | 4 / 4 | 37.77 · 33.18 · 23.47 · 29.99 · 28.70 | 39.39 · 39.45 · 38.51 · 40.95 · 39.12 | 38.51–40.95 | 41.26 · 43.89 · 46.84 · 44.58 · 42.92 | 41.26–46.84 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Zoom in, wheel x1.25 per step | 1e+07 → 1.238e+04 | 4 / 4 | 9.69 · 9.44 · 7.84 · 4.33 · 4.92 | 14.26 · 14.41 · 14.25 · 7.31 · 14.47 | 7.31–14.47 | 36.18 · 39.29 · 30.18 · 26.82 · 36.50 | 26.82–39.29 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Zoom out, wheel x1.25 per step | 1.238e+04 → 1e+07 | 4 / 4 | 9.84 · 9.72 · 5.29 · 4.75 · 9.43 | 14.86 · 15.02 · 14.46 · 9.79 · 15.07 | 9.79–15.07 | 43.19 · 41.70 · 38.45 · 38.72 · 38.48 | 38.45–43.19 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Pan, arrow keys 10 % per step, 1/10 of the record | 1e+06 | 4 / 4 | 17.63 · 15.27 · 10.65 · 9.01 · 9.45 | 18.26 · 18.46 · 17.53 · 14.39 · 17.71 | 14.39–18.46 | 20.03 · 19.31 · 19.81 · 19.46 · 19.53 | 19.31–20.03 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Pan, arrow keys 10 % per step, 1/100 of the record | 1e+05 | 4 / 4 | 11.02 · 10.99 · 5.55 · 10.74 · 5.55 | 11.35 · 11.53 · 11.09 · 11.37 · 9.88 | 9.88–11.53 | 11.73 · 11.96 · 12.11 · 12.44 · 11.77 | 11.73–12.44 | PASS · PASS · PASS · PASS · PASS |
| Full view, redraw only (SmoothSine) | 1e+07 | 4 / 4 | 58.04 · 34.53 · 43.05 · 40.20 · 33.81 | 60.39 · 59.89 · 63.03 · 60.41 · 52.19 | 52.19–63.03 | 63.42 · 63.37 · 66.29 · 65.44 · 63.73 | 63.37–66.29 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Full view, redraw only (BroadbandNoise) | 1e+07 | 4 / 4 | 56.17 · 49.50 · 45.67 · 59.86 · 33.35 | 63.31 · 63.53 · 64.44 · 63.45 · 44.29 | 44.29–64.44 | 65.67 · 68.14 · 69.88 · 67.37 · 64.35 | 64.35–69.88 | FAIL · FAIL · FAIL · FAIL · FAIL |

#### setChannelData()

| Item | Value |
| --- | --- |
| Time per call (4 calls of 10000000 samples) | run 1: min 146.39 ms, median 164.21 ms; run 2: min 152.22 ms, median 175.06 ms; run 3: min 100.92 ms, median 110.45 ms; run 4: min 159.21 ms, median 168.23 ms; run 5: min 139.14 ms, median 158.96 ms |
| Stored samples (8 B timestamp + 8 B value) | 610.4 MiB |
| Pyramid | 81.1 MiB (2.13 B/sample, 13.3 % of the samples) |
| Resident set growth across the 4 calls | run 1: 691.9 MiB; run 2: 691.9 MiB; run 3: 691.8 MiB; run 4: 691.9 MiB; run 5: 691.9 MiB |
