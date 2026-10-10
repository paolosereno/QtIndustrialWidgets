<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
SPDX-License-Identifier: MIT
-->

# StripChart Performance Benchmarks

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
