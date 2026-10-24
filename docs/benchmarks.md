<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
SPDX-License-Identifier: MIT
-->

# StripChart Performance Benchmarks

> `AnalysisChart` figures are in their own part at the end of this page:
> [AnalysisChart Performance Benchmarks](#analysischart-performance-benchmarks).

This page is the single source of performance figures for `QtIndustrialWidgets::StripChart` and
`QtIndustrialWidgets::AnalysisChart`. The figures come from **one benchmark session** (2026-10-22, benchmark source
commit `7d72487`, library 2.3.0 with the changes of #11–#16 and #73): **5 runs** of `bench_StripChart` and 5 runs of
`bench_AnalysisChart`, alternated, on the machine described below, merged with `benchmarks/merge_runs.py`. Every run
is published: each table shows the value of every run (`run 1 · run 2 · …`) and the range of the runs. README,
`docs/strip_chart.md`, `docs/analysis_chart.md` and the CHANGELOG quote this page or link to it. Sections marked
**historical** come from earlier sessions and are kept for reference; they are not part of this session.

All measurements render offscreen into a `QPixmap` with Qt's raster engine: they are **CPU-only** and do not
include window-system composition or vsync.

---

## Targets

Evaluated on this session, every run. A target is marked **PASS** only if it is met in all 5 runs. The targets are
unchanged; the target of #5 was withdrawn by the maintainer after this session, and its rows keep the measurements.

| Target | Measured value, runs 1–5 | Result |
|---|---|---|
| #5: 4 ch × 1M `BroadbandNoise` ≤ **8 ms** per frame (steady-state median), Time | 10.36 · 10.26 · 20.84 · 17.53 · 19.64 ms | **FAIL** (all 5 runs); target withdrawn by the maintainer (2026-10-24, #5): the measured frame time (about 10 ms in the fast mode, 17–21 ms in the slow mode of this machine) judged sufficient |
| #5: same, SampleIndex | 11.70 · 10.38 · 21.00 · 23.65 · 16.29 ms | **FAIL** (all 5 runs); target withdrawn with the Time row (2026-10-24, #5) |
| #6: paint-time growth ≤ **25 %** from 100k to 4M samples (1 ch `BroadbandNoise`, steady state, median of the per-pair ratios of the interleaved case), Time | +6.9 · +6.2 · +6.0 · +5.7 · −3.2 % | **PASS** (all 5 runs) |
| #6: same, SampleIndex | +5.8 · +6.6 · +4.6 · +4.5 · +5.8 % | **PASS** (all 5 runs) |
| `addSample` with autoscale ON ≤ **1 µs** per call (1M samples in the buffer) | 0.0111 · 0.0446 · 0.0324 · 0.0322 · 0.0320 µs | **PASS** (all 5 runs) |
| #25: `AnalysisChart` redraw after a zoom or pan step ≤ **16.7 ms** at p95 (4 ch × 10 M samples) | per case, see the AnalysisChart part below | **FAIL**: 1 of 7 cases passes in all runs (pan at 1/100 of the record) |

Values come from the tables below: *Paint at steady state* (#5), *Paint at steady state, 1 ch x 100k and 1 ch x 4M
interleaved* (#6), *Insertion* (`addSample`) and the AnalysisChart *Redraw* table (#25).

- **#5.** The two modes of this machine (see *Run-to-run variation*) give about 10.3 ms and about 17–21 ms; the
  fast mode is still above 8 ms. In the streaming case the render median of the same case is 10.20 · 10.52 · 20.50 ·
  21.20 · 13.63 ms (Time).
- **#6.** Over the same range the trace pixels grow by +14.9 % in Time mode (434775 → 499675) and +14.9 % in
  SampleIndex mode (439032 → 504541), identical in every run. The growth in time stays below the growth in drawn
  pixels. Run 5 (Time) has a negative growth: in that run the 100k chart has the higher p95 (6.09 against 5.94 ms).

---

## Environment

Printed by the benchmark at start-up (identical in all runs of both benchmarks):

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
| Library version | 2.3.0 |
| Benchmark source commit | 7d72487c5be53e1dc5eb365275c813268bea7c24 |
| Heap allocation counting | malloc/calloc/realloc interposed (glibc) |

Not printed by the benchmark: the compiler flags come from the CMake `Release` configuration
(`-O3 -DNDEBUG -std=c++17 -Wall -Wextra -Wpedantic -fPIC`). The machine runs Ubuntu under WSL2 on a Windows host;
the CPU is a hybrid design (6 performance cores with 2 threads each and 8 efficiency cores, 20 logical CPUs). Every benchmark process is
pinned with `taskset -c 0-11` to the logical CPUs 0–11, the performance cores on this CPU (the benchmark therefore
reports 12 logical CPUs); under WSL2 the mapping of logical CPUs to physical cores is not guaranteed.

---

## Commands

Build (the issue-18 worktree at the benchmark source commit), then the full test suite:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_DOCS=OFF \
      -DBUILD_DESIGNER_PLUGIN=OFF -DBUILD_BENCHMARKS=ON
cmake --build build --clean-first
QT_QPA_PLATFORM=offscreen ctest --test-dir build
```

The session: 5 rounds; each round runs both benchmarks, `bench_StripChart` first in odd rounds and
`bench_AnalysisChart` first in even rounds:

```bash
export QT_QPA_PLATFORM=offscreen
for r in 1 2 3 4 5; do
  if [ $((r % 2)) -eq 1 ]; then order="strip analysis"; else order="analysis strip"; fi
  for b in $order; do
    if [ $b = strip ]; then
      taskset -c 0-11 ./build/bin/bench_StripChart -o strip_run$r.log,txt > strip_run$r.md
    else
      taskset -c 0-11 ./build/bin/bench_AnalysisChart > analysis_run$r.md
    fi
  done
done
python3 benchmarks/merge_runs.py strip_run1.md strip_run2.md strip_run3.md strip_run4.md strip_run5.md > merged_strip.md
python3 benchmarks/merge_runs.py analysis_run1.md analysis_run2.md analysis_run3.md analysis_run4.md analysis_run5.md > merged_analysis.md
```

`merged_strip.md` and `merged_analysis.md` contain the environment and results tables of this page verbatim;
progress lines go to stderr. `merge_runs.py` stops if the runs did not measure the same work (for example a different
number of trace pixels); it did not stop.

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
- **Heap allocations**: counted in the streaming case (every run) by interposing glibc `malloc` / `calloc` / `realloc`
  in the benchmark executable. The count is process-wide: it includes `QPainter`, text and pixmap allocations, not
  only the library's own.
- **Paint at steady state, 100k and 4M interleaved** (#6, #18): a 1 ch × 100k and a 1 ch × 4M `BroadbandNoise`
  chart, in Time and in SampleIndex mode, rendered alternately in the same loop (the order alternates every pair):
  5 warm-up and 30 measured pairs. The growth from 100k to 4M is the median of the 30 per-pair ratios 4M / 100k − 1;
  both charts of a pair see the same machine state, so the bimodal timings below cancel out of it.
- **First paint after resize**: 10 independent trials; each builds a fresh chart, renders at 1000 × 800, resizes to
  1974 × 1080 and times the next render (cold path: decimation streams are rebuilt).
- **Insertion**: real call counts, never extrapolated; 10 trials each. The `addSample` cases run against a ring
  buffer that already holds 1M samples (capacity 1M, so every call evicts one sample).

### Run-to-run variation (bimodal timings)

On this machine render times are **bimodal**: the same case runs either in a slow mode or in a mode roughly twice
as fast, within one run (compare min and median) and between runs (the values of runs 1–5 and their range). The draw workload is the same in
both modes: trace pixel counts are identical between runs, and steady and streaming frames of the same case draw the
same pixels. In fast runs the append time, which involves no rendering, drops as well, which points to the whole
thread running faster (for example scheduling on a different core type of the hybrid CPU under WSL2, or a different
clock frequency). This explanation has not been verified. Small differences between cases on this page are
therefore not meaningful; the targets are evaluated for every run, and the growth target on a measurement that
the two modes do not affect (the interleaved 100k / 4M case).


---

## Results

### bench_StripChart, 5 runs

<!-- bench_StripChart results: begin -->

Merged with `benchmarks/merge_runs.py` from 5 runs (run 1: strip_run1.md, run 2: strip_run2.md, run 3: strip_run3.md, run 4: strip_run4.md, run 5: strip_run5.md).

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
| Library version | 2.3.0 |
| Benchmark source commit | 7d72487c5be53e1dc5eb365275c813268bea7c24 |
| Heap allocation counting | malloc/calloc/realloc interposed (glibc) |

#### Paint at steady state (5 warm-up + 30 measured renders, ms)

| Channels | Samples / ch | Signal | X axis | Trace px | Min, runs 1–5 | Median, runs 1–5 | Median, range | p95, runs 1–5 | p95, range |
| ---: | ---: | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 10k | SmoothSine | Time | 323989 | 2.21 · 2.24 · 2.11 · 2.46 · 2.02 | 2.85 · 2.57 · 2.28 · 2.64 · 2.21 | 2.21–2.85 | 4.05 · 2.98 · 3.51 · 2.85 · 2.89 | 2.85–4.05 |
| 1 | 10k | SmoothSine | SampleIndex | 327157 | 2.12 · 2.17 · 2.18 · 2.49 · 2.04 | 2.22 · 2.41 · 2.43 · 2.71 · 2.23 | 2.22–2.71 | 2.57 · 2.87 · 3.98 · 4.48 · 3.67 | 2.57–4.48 |
| 1 | 10k | BroadbandNoise | Time | 296419 | 2.27 · 2.40 · 2.37 · 2.63 · 2.36 | 2.39 · 2.58 · 2.58 · 2.82 · 2.72 | 2.39–2.82 | 2.80 · 3.68 · 2.92 · 3.52 · 4.18 | 2.80–4.18 |
| 1 | 10k | BroadbandNoise | SampleIndex | 299314 | 2.31 · 2.35 · 2.28 · 2.66 · 2.33 | 2.44 · 2.48 · 2.40 · 2.79 · 2.56 | 2.40–2.79 | 2.79 · 2.72 · 2.73 · 3.26 · 3.65 | 2.72–3.65 |
| 1 | 10k | SpikesAndNaN | Time | 369753 | 2.25 · 2.28 · 2.21 · 2.60 · 2.22 | 2.39 · 2.49 · 2.35 · 2.72 · 2.34 | 2.34–2.72 | 3.03 · 2.73 · 2.72 · 3.20 · 2.97 | 2.72–3.20 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 373362 | 2.16 · 2.35 · 2.27 · 2.58 · 2.15 | 2.42 · 2.51 · 2.36 · 2.76 · 2.37 | 2.36–2.76 | 2.77 · 3.38 · 2.58 · 3.85 · 3.81 | 2.58–3.85 |
| 1 | 100k | SmoothSine | Time | 324321 | 2.15 · 2.13 · 2.14 · 2.40 · 2.08 | 2.26 · 2.23 · 2.22 · 2.57 · 2.24 | 2.22–2.57 | 2.61 · 2.72 · 2.42 · 3.01 · 3.78 | 2.42–3.78 |
| 1 | 100k | SmoothSine | SampleIndex | 327467 | 2.15 · 2.13 · 2.13 · 2.46 · 2.09 | 2.24 · 2.26 · 2.33 · 2.80 · 2.18 | 2.18–2.80 | 2.69 · 2.51 · 2.60 · 3.34 · 2.93 | 2.51–3.34 |
| 1 | 100k | BroadbandNoise | Time | 434775 | 2.81 · 2.83 · 2.84 · 3.19 · 2.82 | 2.93 · 3.05 · 3.02 · 3.33 · 3.01 | 2.93–3.33 | 3.55 · 3.70 · 3.51 · 3.76 · 4.20 | 3.51–4.20 |
| 1 | 100k | BroadbandNoise | SampleIndex | 439032 | 2.82 · 2.87 · 2.80 · 3.16 · 2.80 | 2.98 · 3.01 · 2.88 · 3.33 · 2.94 | 2.88–3.33 | 3.48 · 3.33 · 3.08 · 3.67 · 3.61 | 3.08–3.67 |
| 1 | 100k | SpikesAndNaN | Time | 372842 | 2.27 · 2.17 · 2.21 · 2.66 · 2.22 | 2.42 · 2.40 · 2.39 · 2.89 · 2.33 | 2.33–2.89 | 2.90 · 3.01 · 2.72 · 3.61 · 2.94 | 2.72–3.61 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 376454 | 2.28 · 2.23 · 2.29 · 2.61 · 2.15 | 2.47 · 2.52 · 2.50 · 2.78 · 2.40 | 2.40–2.78 | 3.00 · 3.23 · 2.79 · 3.25 · 3.27 | 2.79–3.27 |
| 1 | 1M | SmoothSine | Time | 325507 | 2.13 · 2.15 · 2.12 · 2.42 · 2.04 | 2.24 · 2.36 · 2.25 · 2.59 · 2.20 | 2.20–2.59 | 2.50 · 2.72 · 2.94 · 2.96 · 2.97 | 2.50–2.97 |
| 1 | 1M | SmoothSine | SampleIndex | 328675 | 2.14 · 2.16 · 2.20 · 2.47 · 2.04 | 2.24 · 2.34 · 2.31 · 2.66 · 2.18 | 2.18–2.66 | 2.68 · 2.94 · 2.85 · 2.86 · 3.03 | 2.68–3.03 |
| 1 | 1M | BroadbandNoise | Time | 488284 | 3.00 · 2.97 · 3.01 · 3.41 · 3.03 | 3.13 · 3.19 · 3.27 · 3.56 · 3.20 | 3.13–3.56 | 3.52 · 3.52 · 3.65 · 4.75 · 3.61 | 3.52–4.75 |
| 1 | 1M | BroadbandNoise | SampleIndex | 493027 | 2.99 · 2.92 · 3.01 · 3.42 · 3.06 | 3.14 · 3.25 · 3.20 · 3.70 · 3.19 | 3.14–3.70 | 3.48 · 3.72 · 4.29 · 5.21 · 3.96 | 3.48–5.21 |
| 1 | 1M | SpikesAndNaN | Time | 352074 | 2.21 · 2.19 · 2.20 · 2.55 · 2.25 | 2.32 · 2.43 · 2.31 · 2.75 · 2.69 | 2.31–2.75 | 2.68 · 3.05 · 2.83 · 2.92 · 3.73 | 2.68–3.73 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 355499 | 2.21 · 2.29 · 2.19 · 2.64 · 2.18 | 2.30 · 2.54 · 2.31 · 2.79 · 2.30 | 2.30–2.79 | 2.49 · 4.09 · 2.63 · 3.17 · 2.88 | 2.49–4.09 |
| 1 | 4M | SmoothSine | Time | 325873 | 2.10 · 2.15 · 2.21 · 2.41 · 2.13 | 2.22 · 2.34 · 2.38 · 2.64 · 2.33 | 2.22–2.64 | 2.71 · 2.64 · 3.06 · 3.19 · 3.08 | 2.64–3.19 |
| 1 | 4M | SmoothSine | SampleIndex | 329047 | 2.09 · 2.12 · 2.12 · 2.43 · 2.14 | 2.23 · 2.28 · 2.24 · 2.66 · 2.26 | 2.23–2.66 | 2.32 · 2.59 · 2.83 · 3.13 · 3.46 | 2.32–3.46 |
| 1 | 4M | BroadbandNoise | Time | 499675 | 2.95 · 2.95 · 3.06 · 3.38 · 3.03 | 3.13 · 3.28 · 3.20 · 3.56 · 3.32 | 3.13–3.56 | 3.53 · 3.62 · 3.58 · 4.07 · 4.92 | 3.53–4.92 |
| 1 | 4M | BroadbandNoise | SampleIndex | 504541 | 3.02 · 3.03 · 3.13 · 3.58 · 3.05 | 3.15 · 3.26 · 3.29 · 3.89 · 3.31 | 3.15–3.89 | 3.38 · 3.51 · 3.63 · 6.16 · 4.16 | 3.38–6.16 |
| 1 | 4M | SpikesAndNaN | Time | 352418 | 2.18 · 2.28 · 2.26 · 2.51 · 2.06 | 2.30 · 2.41 · 2.44 · 2.75 · 2.39 | 2.30–2.75 | 2.77 · 2.61 · 3.83 · 4.13 · 3.40 | 2.61–4.13 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 355860 | 2.21 · 2.28 · 2.24 · 2.51 · 2.12 | 2.29 · 2.51 · 2.41 · 2.79 · 2.27 | 2.27–2.79 | 2.49 · 2.90 · 2.70 · 3.45 · 3.14 | 2.49–3.45 |
| 4 | 10k | SmoothSine | Time | 1090434 | 6.19 · 6.37 · 6.35 · 9.05 · 6.32 | 6.63 · 7.20 · 6.98 · 13.43 · 6.95 | 6.63–13.43 | 10.40 · 10.75 · 12.16 · 15.17 · 11.01 | 10.40–15.17 |
| 4 | 10k | SmoothSine | SampleIndex | 1101066 | 6.51 · 6.42 · 7.77 · 7.32 · 6.27 | 7.15 · 7.55 · 12.34 · 8.11 · 6.95 | 6.95–12.34 | 11.53 · 10.53 · 14.07 · 13.74 · 10.26 | 10.26–14.07 |
| 4 | 10k | BroadbandNoise | Time | 975135 | 7.21 · 7.27 · 8.72 · 8.80 · 7.16 | 7.99 · 7.90 · 15.20 · 16.46 · 7.74 | 7.74–16.46 | 11.03 · 8.54 · 21.21 · 18.36 · 9.12 | 8.54–21.21 |
| 4 | 10k | BroadbandNoise | SampleIndex | 984647 | 7.30 · 7.40 · 7.46 · 8.62 · 7.24 | 7.67 · 7.91 · 9.33 · 16.25 · 7.51 | 7.51–16.25 | 9.53 · 10.81 · 14.84 · 17.44 · 9.30 | 9.30–17.44 |
| 4 | 10k | SpikesAndNaN | Time | 1122137 | 6.77 · 6.89 · 6.89 · 8.27 · 6.93 | 7.25 · 7.93 · 9.21 · 12.98 · 8.38 | 7.25–12.98 | 8.50 · 10.10 · 15.31 · 16.16 · 10.56 | 8.50–16.16 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 1133084 | 6.72 · 6.91 · 6.80 · 8.47 · 6.85 | 7.06 · 7.82 · 10.19 · 15.49 · 10.57 | 7.06–15.49 | 7.61 · 9.81 · 15.94 · 17.56 · 12.85 | 7.61–17.56 |
| 4 | 100k | SmoothSine | Time | 1092584 | 6.16 · 6.22 · 6.23 · 7.20 · 7.86 | 7.41 · 6.56 · 7.17 · 9.99 · 12.31 | 6.56–12.31 | 14.19 · 8.78 · 12.44 · 14.71 · 13.39 | 8.78–14.71 |
| 4 | 100k | SmoothSine | SampleIndex | 1103201 | 6.27 · 6.22 · 6.41 · 7.16 · 6.22 | 6.95 · 6.87 · 12.16 · 12.55 · 6.67 | 6.67–12.55 | 9.76 · 8.17 · 19.90 · 15.18 · 8.47 | 8.17–19.90 |
| 4 | 100k | BroadbandNoise | Time | 1281456 | 9.03 · 9.12 · 14.08 · 18.10 · 17.06 | 10.20 · 10.02 · 19.27 · 22.11 · 19.54 | 10.02–22.11 | 14.49 · 11.69 · 21.20 · 24.15 · 20.06 | 11.69–24.15 |
| 4 | 100k | BroadbandNoise | SampleIndex | 1293980 | 9.18 · 9.16 · 9.16 · 10.18 · 9.22 | 9.95 · 9.62 · 19.34 · 20.06 · 17.43 | 9.62–20.06 | 12.80 · 10.85 · 22.40 · 23.02 · 19.96 | 10.85–23.02 |
| 4 | 100k | SpikesAndNaN | Time | 1133272 | 6.76 · 6.68 · 7.02 · 8.19 · 6.80 | 7.09 · 7.13 · 7.73 · 13.70 · 8.14 | 7.09–13.70 | 8.51 · 8.09 · 9.66 · 17.22 · 14.11 | 8.09–17.22 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 1144290 | 6.90 · 6.82 · 6.74 · 7.95 · 6.92 | 7.63 · 7.22 · 8.07 · 12.22 · 11.75 | 7.22–12.22 | 13.15 · 8.37 · 13.67 · 16.37 · 13.25 | 8.37–16.37 |
| 4 | 1M | SmoothSine | Time | 1096168 | 6.25 · 6.37 · 6.26 · 7.76 · 6.77 | 6.79 · 6.88 · 6.87 · 8.67 · 11.60 | 6.79–11.60 | 8.02 · 7.51 · 7.94 · 15.35 · 12.83 | 7.51–15.35 |
| 4 | 1M | SmoothSine | SampleIndex | 1106814 | 6.33 · 6.11 · 6.28 · 7.56 · 7.26 | 7.01 · 6.74 · 7.18 · 8.64 · 12.06 | 6.74–12.06 | 9.01 · 7.77 · 11.42 · 11.36 · 13.38 | 7.77–13.38 |
| 4 | 1M | BroadbandNoise | Time | 1366742 | 9.88 · 9.85 · 10.87 · 11.27 · 11.13 | 10.36 · 10.26 · 20.84 · 17.53 · 19.64 | 10.26–20.84 | 14.24 · 11.90 · 22.74 · 23.86 · 21.16 | 11.90–23.86 |
| 4 | 1M | BroadbandNoise | SampleIndex | 1379992 | 9.84 · 9.77 · 14.35 · 18.76 · 9.86 | 11.70 · 10.38 · 21.00 · 23.65 · 16.29 | 10.38–23.65 | 17.01 · 11.81 · 23.36 · 25.27 · 27.36 | 11.81–27.36 |
| 4 | 1M | SpikesAndNaN | Time | 1108617 | 6.60 · 6.70 · 7.41 · 7.85 · 16.62 | 7.97 · 7.54 · 12.77 · 11.35 · 17.81 | 7.54–17.81 | 10.34 · 9.45 · 14.62 · 15.11 · 20.34 | 9.45–20.34 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 1119375 | 6.62 · 6.56 · 6.58 · 7.51 · 6.70 | 6.98 · 6.90 · 11.63 · 9.97 · 13.79 | 6.90–13.79 | 9.31 · 9.16 · 13.05 · 14.16 · 17.70 | 9.16–17.70 |
| 4 | 4M | SmoothSine | Time | 1097122 | 6.16 · 6.11 · 6.29 · 7.16 · 7.98 | 6.46 · 6.34 · 7.69 · 7.71 · 18.98 | 6.34–18.98 | 7.67 · 6.98 · 13.45 · 10.71 · 22.38 | 6.98–22.38 |
| 4 | 4M | SmoothSine | SampleIndex | 1107817 | 6.25 · 6.21 · 7.92 · 7.46 · 9.51 | 6.82 · 6.44 · 12.43 · 10.45 · 16.84 | 6.44–16.84 | 8.15 · 6.79 · 13.53 · 13.04 · 22.50 | 6.79–22.50 |
| 4 | 4M | BroadbandNoise | Time | 1382209 | 10.03 · 10.31 · 15.40 · 11.64 · 11.70 | 11.91 · 11.14 · 19.90 · 22.50 · 18.09 | 11.14–22.50 | 17.75 · 13.09 · 24.11 · 24.36 · 22.95 | 13.09–24.36 |
| 4 | 4M | BroadbandNoise | SampleIndex | 1395684 | 10.05 · 10.12 · 15.18 · 18.53 · 10.14 | 11.26 · 10.92 · 22.00 · 24.07 · 14.78 | 10.92–24.07 | 14.96 · 12.62 · 23.60 · 27.54 · 20.04 | 12.62–27.54 |
| 4 | 4M | SpikesAndNaN | Time | 1109534 | 6.53 · 7.26 · 7.17 · 8.05 · 6.39 | 6.97 · 8.19 · 10.64 · 10.86 · 8.12 | 6.97–10.86 | 8.49 · 10.62 · 14.32 · 14.82 · 12.27 | 8.49–14.82 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 1120348 | 6.58 · 7.25 · 7.55 · 8.91 · 6.52 | 8.14 · 8.16 · 9.10 · 11.99 · 8.22 | 8.14–11.99 | 11.77 · 9.52 · 12.76 · 15.64 · 13.32 | 9.52–15.64 |

#### Paint at steady state, 1 ch x 100k and 1 ch x 4M interleaved (rendered alternately in the same loop; 5 warm-up + 30 measured pairs, ms; growth = per-pair ratio 4M / 100k - 1)

| Channels | Signal | X axis | Trace px 100k | Trace px 4M | 100k median, runs 1–5 | 100k median, range | 100k p95, runs 1–5 | 100k p95, range | 4M median, runs 1–5 | 4M median, range | 4M p95, runs 1–5 | 4M p95, range | Growth, median of per-pair ratios (%), runs 1–5 | Growth, median of per-pair ratios (%), range |
| ---: | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | BroadbandNoise | Time | 434775 | 499675 | 3.27 · 3.15 · 3.08 · 3.31 · 3.85 | 3.08–3.85 | 3.50 · 3.44 · 3.40 · 3.61 · 6.09 | 3.40–6.09 | 3.45 · 3.28 · 3.29 · 3.51 · 3.53 | 3.28–3.53 | 3.75 · 4.00 · 3.74 · 3.75 · 5.94 | 3.74–5.94 | 6.9 · 6.2 · 6.0 · 5.7 · -3.2 | -3.2–6.9 |
| 1 | BroadbandNoise | SampleIndex | 439032 | 504541 | 3.05 · 3.11 · 3.10 · 3.38 · 3.44 | 3.05–3.44 | 3.43 · 3.49 · 3.50 · 3.59 · 5.41 | 3.43–5.41 | 3.17 · 3.32 · 3.29 · 3.54 · 3.50 | 3.17–3.54 | 3.68 · 3.58 · 3.53 · 3.73 · 5.62 | 3.53–5.62 | 5.8 · 6.6 · 4.6 · 4.5 · 5.8 | 4.5–6.6 |

#### Paint with streaming (1/60 s of data appended to every channel before each render; 5 warm-up + 30 measured frames, ms)

| Channels | Samples / ch | Signal | X axis | New samples / ch / frame | Trace px | Render min, runs 1–5 | Render median, runs 1–5 | Render median, range | Render p95, runs 1–5 | Render p95, range | Append median (all ch), runs 1–5 | Append median (all ch), range |
| ---: | ---: | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 10k | SmoothSine | Time | 17 | 323783 | 2.14 · 2.22 · 2.15 · 2.38 · 2.22 | 2.31 · 2.39 · 2.31 · 2.57 · 2.35 | 2.31–2.57 | 2.75 · 3.63 · 2.78 · 2.95 · 3.96 | 2.75–3.96 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 10k | SmoothSine | SampleIndex | 17 | 326949 | 2.15 · 2.19 · 2.15 · 2.42 · 2.23 | 2.47 · 2.39 · 2.27 · 2.57 · 2.37 | 2.27–2.57 | 2.81 · 3.34 · 2.48 · 2.78 · 4.63 | 2.48–4.63 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 10k | BroadbandNoise | Time | 17 | 296412 | 2.36 · 2.38 · 2.36 · 2.64 · 2.41 | 2.70 · 2.50 · 2.48 · 2.74 · 2.71 | 2.48–2.74 | 2.95 · 2.66 · 2.67 · 2.99 · 4.01 | 2.66–4.01 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 10k | BroadbandNoise | SampleIndex | 17 | 299291 | 2.31 · 2.34 · 2.37 · 2.63 · 2.40 | 2.58 · 2.49 · 2.46 · 2.81 · 2.59 | 2.46–2.81 | 2.90 · 2.62 · 2.62 · 3.07 · 5.24 | 2.62–5.24 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 10k | SpikesAndNaN | Time | 17 | 369072 | 2.23 · 2.32 · 2.34 · 2.55 · 3.51 | 2.61 · 2.57 · 2.43 · 2.72 · 5.03 | 2.43–5.03 | 2.84 · 3.23 · 2.66 · 3.02 · 7.59 | 2.66–7.59 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 17 | 372671 | 2.26 · 2.43 · 2.36 · 2.58 · 2.44 | 2.62 · 2.66 · 2.46 · 2.71 · 2.71 | 2.46–2.71 | 2.97 · 2.97 · 3.05 · 3.01 · 5.31 | 2.97–5.31 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 100k | SmoothSine | Time | 167 | 324110 | 2.16 · 2.23 · 2.08 · 2.40 · 2.13 | 2.52 · 2.44 · 2.27 · 2.65 · 2.64 | 2.27–2.65 | 2.80 · 5.27 · 2.64 · 3.05 · 5.57 | 2.64–5.57 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 100k | SmoothSine | SampleIndex | 167 | 327253 | 2.16 · 2.33 · 2.21 · 2.40 · 2.14 | 2.54 · 2.52 · 2.33 · 2.62 · 2.35 | 2.33–2.62 | 2.88 · 3.16 · 2.61 · 2.96 · 4.14 | 2.61–4.14 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 100k | BroadbandNoise | Time | 167 | 434566 | 2.84 · 2.95 · 2.85 · 3.15 · 2.84 | 3.19 · 3.14 · 2.96 · 3.33 · 3.12 | 2.96–3.33 | 3.43 · 3.57 · 3.39 · 3.53 · 4.75 | 3.39–4.75 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 100k | BroadbandNoise | SampleIndex | 167 | 438820 | 2.86 · 2.96 · 2.90 · 3.13 · 2.90 | 3.21 · 3.18 · 3.10 · 3.35 · 4.54 | 3.10–4.54 | 3.53 · 3.95 · 3.61 · 3.69 · 6.01 | 3.53–6.01 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 100k | SpikesAndNaN | Time | 167 | 372051 | 2.25 · 2.52 · 2.23 · 2.59 · 2.28 | 2.35 · 2.77 · 2.41 · 2.70 · 3.10 | 2.35–3.10 | 2.48 · 3.18 · 2.92 · 2.85 · 6.70 | 2.48–6.70 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 167 | 375656 | 2.24 · 2.44 · 2.36 · 2.59 · 2.45 | 2.41 · 2.73 · 2.47 · 2.77 · 3.81 | 2.41–3.81 | 2.68 · 3.14 · 3.05 · 3.18 · 5.36 | 2.68–5.36 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 1 | 1M | SmoothSine | Time | 1667 | 325247 | 2.12 · 2.22 · 2.11 · 2.41 · 2.18 | 2.33 · 2.43 · 2.23 · 2.53 · 2.51 | 2.23–2.53 | 2.87 · 2.78 · 2.57 · 2.80 · 4.74 | 2.57–4.74 | 0.01 · 0.01 · 0.01 · 0.01 · 0.01 | 0.01–0.01 |
| 1 | 1M | SmoothSine | SampleIndex | 1667 | 328415 | 2.13 · 2.18 · 2.16 · 2.43 · 2.15 | 2.21 · 2.38 · 2.24 · 2.60 · 2.39 | 2.21–2.60 | 2.71 · 2.66 · 2.50 · 2.82 · 4.47 | 2.50–4.47 | 0.01 · 0.01 · 0.01 · 0.01 · 0.01 | 0.01–0.01 |
| 1 | 1M | BroadbandNoise | Time | 1667 | 488110 | 2.90 · 3.09 · 2.97 · 3.33 · 3.00 | 3.06 · 3.30 · 3.09 · 3.48 · 3.14 | 3.06–3.48 | 3.61 · 3.85 · 3.41 · 3.74 · 4.42 | 3.41–4.42 | 0.01 · 0.01 · 0.01 · 0.01 · 0.01 | 0.01–0.01 |
| 1 | 1M | BroadbandNoise | SampleIndex | 1667 | 492836 | 2.94 · 3.12 · 2.98 · 3.33 · 2.96 | 3.19 · 3.38 · 3.20 · 3.49 · 3.21 | 3.19–3.49 | 3.76 · 3.74 · 3.53 · 3.83 · 6.78 | 3.53–6.78 | 0.01 · 0.01 · 0.01 · 0.01 · 0.01 | 0.01–0.01 |
| 1 | 1M | SpikesAndNaN | Time | 1667 | 352488 | 2.17 · 2.31 · 2.22 · 2.45 · 2.22 | 2.34 · 2.48 · 2.29 · 2.64 · 2.41 | 2.29–2.64 | 3.01 · 3.13 · 2.67 · 2.82 · 3.36 | 2.67–3.36 | 0.01 · 0.01 · 0.01 · 0.01 · 0.01 | 0.01–0.01 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 1667 | 355922 | 2.21 · 2.32 · 2.23 · 2.52 · 2.25 | 2.33 · 2.66 · 2.40 · 2.65 · 2.37 | 2.33–2.66 | 2.66 · 3.22 · 2.72 · 3.19 · 4.39 | 2.66–4.39 | 0.01 · 0.01 · 0.01 · 0.01 · 0.01 | 0.01–0.01 |
| 1 | 4M | SmoothSine | Time | 6667 | 325601 | 2.08 · 2.20 · 2.12 · 2.43 · 2.21 | 2.31 · 2.36 · 2.27 · 2.61 · 2.42 | 2.27–2.61 | 2.62 · 2.64 · 3.34 · 2.79 · 4.77 | 2.62–4.77 | 0.03 · 0.03 · 0.03 · 0.03 · 0.03 | 0.03–0.03 |
| 1 | 4M | SmoothSine | SampleIndex | 6667 | 328780 | 2.15 · 2.21 · 2.10 · 2.42 · 2.16 | 2.24 · 2.31 · 2.21 · 2.55 · 2.34 | 2.21–2.55 | 4.51 · 2.61 · 2.43 · 3.01 · 3.64 | 2.43–4.51 | 0.03 · 0.03 · 0.03 · 0.03 · 0.03 | 0.03–0.03 |
| 1 | 4M | BroadbandNoise | Time | 6667 | 499513 | 3.03 · 3.09 · 2.98 · 3.33 · 3.07 | 3.20 · 3.22 · 3.30 · 3.47 · 3.31 | 3.20–3.47 | 3.70 · 3.63 · 5.35 · 3.80 · 5.20 | 3.63–5.35 | 0.03 · 0.03 · 0.03 · 0.03 · 0.03 | 0.03–0.03 |
| 1 | 4M | BroadbandNoise | SampleIndex | 6667 | 504369 | 3.02 · 3.04 · 3.05 · 3.46 · 3.08 | 3.23 · 3.29 · 3.24 · 3.68 · 3.42 | 3.23–3.68 | 3.63 · 3.69 · 6.34 · 6.75 · 5.06 | 3.63–6.75 | 0.03 · 0.03 · 0.03 · 0.04 · 0.03 | 0.03–0.04 |
| 1 | 4M | SpikesAndNaN | Time | 6667 | 352822 | 2.22 · 2.27 · 2.25 · 2.52 · 2.22 | 2.36 · 2.46 · 2.58 · 2.73 · 2.39 | 2.36–2.73 | 2.82 · 2.82 · 3.82 · 3.02 · 3.43 | 2.82–3.82 | 0.03 · 0.03 · 0.03 · 0.03 · 0.03 | 0.03–0.03 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 6667 | 356275 | 2.20 · 2.23 · 2.30 · 2.47 · 2.16 | 2.36 · 2.39 · 2.49 · 2.73 · 2.42 | 2.36–2.73 | 2.57 · 2.62 · 3.81 · 3.22 · 3.91 | 2.57–3.91 | 0.03 · 0.03 · 0.03 · 0.03 · 0.03 | 0.03–0.03 |
| 4 | 10k | SmoothSine | Time | 17 | 1090856 | 6.31 · 6.19 · 6.23 · 7.28 · 6.05 | 6.68 · 6.81 · 6.79 · 7.85 · 7.61 | 6.68–7.85 | 7.83 · 7.78 · 9.66 · 11.14 · 13.18 | 7.78–13.18 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 4 | 10k | SmoothSine | SampleIndex | 17 | 1101494 | 6.27 · 6.69 · 6.33 · 7.34 · 6.32 | 6.73 · 7.12 · 9.97 · 9.51 · 6.96 | 6.73–9.97 | 8.24 · 8.51 · 12.71 · 12.64 · 10.62 | 8.24–12.71 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 4 | 10k | BroadbandNoise | Time | 17 | 974968 | 7.22 · 7.51 · 10.10 · 8.34 · 7.23 | 7.63 · 7.89 · 14.81 · 11.86 · 7.72 | 7.63–14.81 | 8.90 · 8.48 · 16.05 · 14.17 · 10.81 | 8.48–16.05 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 4 | 10k | BroadbandNoise | SampleIndex | 17 | 984456 | 7.21 · 7.39 · 7.20 · 8.09 · 7.28 | 7.91 · 7.86 · 7.98 · 11.47 · 8.06 | 7.86–11.47 | 8.90 · 8.79 · 11.01 · 16.67 · 12.93 | 8.79–16.67 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 4 | 10k | SpikesAndNaN | Time | 17 | 1122588 | 6.81 · 6.67 · 7.96 · 7.78 · 6.74 | 7.86 · 7.04 · 13.25 · 9.10 · 7.83 | 7.04–13.25 | 8.99 · 8.33 · 14.50 · 12.69 · 13.86 | 8.33–14.50 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 17 | 1133534 | 6.79 · 6.88 · 6.89 · 7.91 · 6.77 | 7.75 · 7.49 · 13.01 · 14.97 · 7.27 | 7.27–14.97 | 10.73 · 8.70 · 17.89 · 17.21 · 15.54 | 8.70–17.89 | 0.00 · 0.00 · 0.00 · 0.00 · 0.00 | 0.00–0.00 |
| 4 | 100k | SmoothSine | Time | 167 | 1092663 | 6.15 · 6.47 · 6.20 · 7.23 · 6.26 | 6.42 · 7.06 · 7.53 · 9.39 · 6.95 | 6.42–9.39 | 7.33 · 10.65 · 12.15 · 13.80 · 10.00 | 7.33–13.80 | 0.01 · 0.01 · 0.01 · 0.02 · 0.01 | 0.01–0.02 |
| 4 | 100k | SmoothSine | SampleIndex | 167 | 1103273 | 6.19 · 6.32 · 6.49 · 7.13 · 6.31 | 6.53 · 7.03 · 7.30 · 9.46 · 7.14 | 6.53–9.46 | 7.41 · 7.89 · 12.52 · 13.09 · 11.54 | 7.41–13.09 | 0.01 · 0.01 · 0.01 · 0.02 · 0.01 | 0.01–0.02 |
| 4 | 100k | BroadbandNoise | Time | 167 | 1281303 | 9.00 · 9.31 · 9.56 · 13.88 · 9.07 | 9.65 · 10.00 · 18.54 · 19.76 · 9.41 | 9.41–19.76 | 17.48 · 15.92 · 24.40 · 22.71 · 11.68 | 11.68–24.40 | 0.01 · 0.01 · 0.02 · 0.02 · 0.01 | 0.01–0.02 |
| 4 | 100k | BroadbandNoise | SampleIndex | 167 | 1293837 | 9.11 · 9.61 · 16.04 · 13.89 · 9.08 | 10.20 · 10.45 · 19.71 · 20.78 · 10.10 | 10.10–20.78 | 13.65 · 16.01 · 23.70 · 23.34 · 14.11 | 13.65–23.70 | 0.01 · 0.01 · 0.02 · 0.02 · 0.01 | 0.01–0.02 |
| 4 | 100k | SpikesAndNaN | Time | 167 | 1133367 | 6.87 · 6.94 · 9.26 · 8.40 · 6.71 | 7.73 · 7.78 · 13.13 · 14.83 · 7.17 | 7.17–14.83 | 8.98 · 11.92 · 14.07 · 19.21 · 9.15 | 8.98–19.21 | 0.01 · 0.01 · 0.01 · 0.02 · 0.01 | 0.01–0.02 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 167 | 1144386 | 6.85 · 7.18 · 9.27 · 7.28 · 6.82 | 7.04 · 7.87 · 13.24 · 8.80 · 7.57 | 7.04–13.24 | 9.09 · 11.62 · 14.25 · 15.89 · 10.24 | 9.09–15.89 | 0.01 · 0.01 · 0.02 · 0.01 · 0.01 | 0.01–0.02 |
| 4 | 1M | SmoothSine | Time | 1667 | 1096052 | 6.15 · 6.80 · 6.20 · 6.92 · 6.14 | 6.48 · 7.61 · 9.03 · 8.62 · 6.58 | 6.48–9.03 | 7.73 · 10.73 · 13.26 · 10.03 · 8.03 | 7.73–13.26 | 0.03 · 0.03 · 0.08 · 0.13 · 0.03 | 0.03–0.13 |
| 4 | 1M | SmoothSine | SampleIndex | 1667 | 1106706 | 6.26 · 6.93 · 6.62 · 8.36 · 6.14 | 7.62 · 8.44 · 12.09 · 12.38 · 6.51 | 6.51–12.38 | 10.97 · 14.79 · 17.63 · 13.20 · 8.66 | 8.66–17.63 | 0.03 · 0.03 · 0.13 · 0.13 · 0.04 | 0.03–0.13 |
| 4 | 1M | BroadbandNoise | Time | 1667 | 1366905 | 9.59 · 9.85 · 11.13 · 13.64 · 10.53 | 10.20 · 10.52 · 20.50 · 21.20 · 13.63 | 10.20–21.20 | 20.26 · 13.89 · 28.91 · 24.71 · 21.54 | 13.89–28.91 | 0.04 · 0.04 · 0.13 · 0.13 · 0.10 | 0.04–0.13 |
| 4 | 1M | BroadbandNoise | SampleIndex | 1667 | 1380153 | 9.79 · 9.95 · 9.87 · 12.31 · 9.80 | 10.91 · 10.63 · 20.37 · 21.43 · 11.23 | 10.63–21.43 | 16.66 · 12.44 · 22.71 · 23.91 · 13.80 | 12.44–23.91 | 0.04 · 0.04 · 0.13 · 0.13 · 0.04 | 0.04–0.13 |
| 4 | 1M | SpikesAndNaN | Time | 1667 | 1108032 | 6.54 · 6.97 · 7.14 · 6.26 · 6.52 | 7.08 · 7.50 · 12.98 · 10.67 · 7.12 | 7.08–12.98 | 8.75 · 9.64 · 16.78 · 13.33 · 8.99 | 8.75–16.78 | 0.03 · 0.03 · 0.13 · 0.13 · 0.03 | 0.03–0.13 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 1667 | 1118797 | 6.57 · 6.62 · 6.89 · 6.95 · 6.80 | 7.34 · 7.57 · 12.74 · 9.67 · 7.21 | 7.21–12.74 | 13.85 · 10.18 · 13.48 · 14.25 · 9.79 | 9.79–14.25 | 0.03 · 0.03 · 0.13 · 0.13 · 0.04 | 0.03–0.13 |
| 4 | 4M | SmoothSine | Time | 6667 | 1096974 | 6.25 · 6.29 · 6.30 · 6.90 · 6.18 | 6.59 · 7.28 · 8.83 · 9.71 · 6.65 | 6.59–9.71 | 8.95 · 11.45 · 12.58 · 11.03 · 10.66 | 8.95–12.58 | 0.13 · 0.13 · 0.13 · 0.50 · 0.12 | 0.12–0.50 |
| 4 | 4M | SmoothSine | SampleIndex | 6667 | 1107695 | 6.14 · 6.45 · 6.62 · 8.33 · 6.19 | 6.35 · 6.69 · 12.15 · 10.28 · 7.16 | 6.35–12.15 | 8.38 · 7.41 · 13.01 · 13.57 · 12.96 | 7.41–13.57 | 0.12 · 0.12 · 0.50 · 0.50 · 0.12 | 0.12–0.50 |
| 4 | 4M | BroadbandNoise | Time | 6667 | 1382458 | 9.77 · 10.13 · 10.89 · 10.47 · 9.89 | 10.65 · 11.97 · 21.38 · 15.37 · 10.86 | 10.65–21.38 | 13.73 · 15.58 · 24.29 · 23.07 · 13.24 | 13.24–24.29 | 0.13 · 0.15 · 0.50 · 0.24 · 0.15 | 0.13–0.50 |
| 4 | 4M | BroadbandNoise | SampleIndex | 6667 | 1395925 | 9.83 · 9.95 · 10.42 · 10.06 · 9.93 | 10.26 · 10.83 · 20.98 · 20.25 · 11.02 | 10.26–20.98 | 12.01 · 13.74 · 24.04 · 24.26 · 18.27 | 12.01–24.26 | 0.12 · 0.13 · 0.50 · 0.49 · 0.14 | 0.12–0.50 |
| 4 | 4M | SpikesAndNaN | Time | 6667 | 1108921 | 6.43 · 6.60 · 6.51 · 7.35 · 6.55 | 6.87 · 7.03 · 10.05 · 13.46 · 7.25 | 6.87–13.46 | 9.54 · 8.32 · 13.64 · 14.36 · 9.12 | 8.32–14.36 | 0.12 · 0.13 · 0.50 · 0.50 · 0.12 | 0.12–0.50 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 6667 | 1119754 | 6.58 · 6.61 · 6.77 · 8.10 · 6.76 | 7.12 · 7.12 · 12.42 · 13.56 · 8.01 | 7.12–13.56 | 8.23 · 7.85 · 18.33 · 15.11 · 15.41 | 7.85–18.33 | 0.14 · 0.12 · 0.50 · 0.51 · 0.14 | 0.12–0.51 |

#### Heap allocations in the streaming case (per measured frame; median / max over 30 frames)

| Channels | Samples / ch | Signal | X axis | Allocations per render, runs 1–5 | KiB allocated per render, runs 1–5 | Allocations per append (all ch), runs 1–5 |
| ---: | ---: | --- | --- | ---: | ---: | ---: |
| 1 | 10k | SmoothSine | Time | 125 / 131 · 123 / 132 · 127 / 132 · 124 / 130 · 124 / 131 | 138.1 / 150.2 · 134.0 / 146.6 · 141.1 / 150.2 · 136.0 / 148.1 · 136.0 / 150.1 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 10k | SmoothSine | SampleIndex | 127 / 131 · 124 / 130 · 127 / 131 · 125 / 130 · 125 / 131 | 142.1 / 150.2 · 135.0 / 148.1 · 142.1 / 150.2 · 137.0 / 148.1 · 138.1 / 150.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 10k | BroadbandNoise | Time | 126 / 133 · 126 / 133 · 126 / 132 · 125 / 129 · 126 / 132 | 266.1 / 280.2 · 266.1 / 280.2 · 266.1 / 278.2 · 264.0 / 272.1 · 265.0 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 10k | BroadbandNoise | SampleIndex | 126 / 133 · 125 / 132 · 127 / 133 · 126 / 130 · 125 / 132 | 266.1 / 280.2 · 264.0 / 278.2 · 268.1 / 280.2 · 265.0 / 274.1 · 264.0 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 10k | SpikesAndNaN | Time | 143 / 158 · 142 / 159 · 144 / 165 · 144 / 158 · 139 / 154 | 362.1 / 467.5 · 360.0 / 469.5 · 364.1 / 481.6 · 363.1 / 467.5 · 354.0 / 459.5 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 10k | SpikesAndNaN | SampleIndex | 143 / 159 · 140 / 159 · 144 / 164 · 143 / 155 · 141 / 155 | 362.1 / 469.5 · 356.0 / 469.5 · 364.1 / 479.6 · 361.0 / 461.5 · 357.0 / 461.5 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 100k | SmoothSine | Time | 127 / 133 · 124 / 128 · 127 / 132 · 125 / 131 · 123 / 131 | 267.1 / 280.2 · 262.0 / 270.1 · 268.1 / 278.2 · 263.0 / 276.1 · 260.0 / 276.1 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 100k | SmoothSine | SampleIndex | 126 / 132 · 123 / 131 · 125 / 129 · 124 / 128 · 125 / 132 | 266.1 / 278.1 · 260.0 / 276.1 · 264.0 / 272.1 · 262.0 / 270.1 · 264.0 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 100k | BroadbandNoise | Time | 127 / 131 · 123 / 128 · 125 / 132 · 125 / 129 · 125 / 130 | 268.1 / 276.1 · 260.0 / 270.1 · 264.0 / 278.2 · 264.0 / 272.1 · 263.0 / 274.1 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 100k | BroadbandNoise | SampleIndex | 127 / 130 · 123 / 129 · 126 / 133 · 126 / 132 · 122 / 129 | 268.1 / 274.1 · 260.0 / 272.1 · 265.0 / 280.2 · 265.0 / 278.2 · 258.0 / 272.1 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 100k | SpikesAndNaN | Time | 143 / 158 · 140 / 155 · 145 / 155 · 142 / 158 · 139 / 154 | 361.0 / 467.5 · 356.0 / 461.5 · 366.1 / 461.5 · 360.0 / 467.5 · 354.0 / 459.5 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 100k | SpikesAndNaN | SampleIndex | 145 / 165 · 141 / 158 · 144 / 161 · 143 / 162 · 139 / 155 | 366.1 / 481.6 · 358.0 / 467.5 · 363.1 / 473.6 · 361.0 / 475.6 · 354.0 / 461.5 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 1M | SmoothSine | Time | 127 / 132 · 125 / 132 · 126 / 131 · 127 / 131 · 125 / 132 | 268.1 / 278.2 · 264.0 / 278.2 · 266.1 / 276.1 · 268.1 / 276.1 · 264.0 / 278.1 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 1M | SmoothSine | SampleIndex | 128 / 132 · 125 / 132 · 128 / 132 · 127 / 132 · 127 / 132 | 270.1 / 278.1 · 264.0 / 278.2 · 269.1 / 278.2 · 268.1 / 278.2 · 268.1 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 1M | BroadbandNoise | Time | 125 / 131 · 125 / 128 · 127 / 133 · 124 / 130 · 127 / 132 | 264.0 / 276.1 · 263.0 / 270.1 · 268.1 / 280.2 · 262.0 / 274.1 · 267.1 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 1M | BroadbandNoise | SampleIndex | 127 / 132 · 124 / 130 · 125 / 131 · 124 / 130 · 125 / 132 | 267.1 / 278.2 · 262.0 / 274.1 · 264.0 / 276.1 · 262.0 / 274.1 · 263.0 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 1M | SpikesAndNaN | Time | 164 / 180 · 163 / 175 · 165 / 176 · 165 / 177 · 164 / 180 | 682.1 / 793.6 · 680.0 / 783.5 · 684.1 / 785.5 · 683.1 / 787.5 · 684.1 / 793.6 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 1M | SpikesAndNaN | SampleIndex | 162 / 180 · 162 / 174 · 165 / 179 · 163 / 181 · 163 / 178 | 680.0 / 793.6 · 679.0 / 781.5 · 686.1 / 791.6 · 682.1 / 795.6 · 682.1 / 789.5 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 4M | SmoothSine | Time | 128 / 132 · 124 / 132 · 126 / 130 · 127 / 132 · 125 / 131 | 270.1 / 278.2 · 262.0 / 278.1 · 266.1 / 274.1 · 267.1 / 278.2 · 264.0 / 276.1 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 4M | SmoothSine | SampleIndex | 125 / 132 · 127 / 132 · 126 / 132 · 125 / 130 · 128 / 132 | 264.0 / 278.2 · 267.1 / 278.2 · 265.0 / 278.2 · 264.0 / 274.1 · 269.1 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 4M | BroadbandNoise | Time | 127 / 132 · 123 / 130 · 126 / 129 · 126 / 132 · 125 / 132 | 268.1 / 278.2 · 260.0 / 274.1 · 266.1 / 272.1 · 266.1 / 278.2 · 263.0 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 4M | BroadbandNoise | SampleIndex | 126 / 132 · 123 / 131 · 125 / 130 · 124 / 130 · 123 / 132 | 266.1 / 278.2 · 260.0 / 276.1 · 264.0 / 274.1 · 262.0 / 274.1 · 260.0 / 278.2 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 4M | SpikesAndNaN | Time | 163 / 180 · 162 / 178 · 162 / 179 · 164 / 182 · 164 / 179 | 682.1 / 793.6 · 679.0 / 789.5 · 678.0 / 791.6 · 684.1 / 797.6 · 684.1 / 791.6 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 1 | 4M | SpikesAndNaN | SampleIndex | 164 / 175 · 164 / 176 · 163 / 179 · 163 / 178 · 163 / 183 | 684.1 / 783.5 · 684.1 / 785.5 · 681.0 / 791.6 · 682.1 / 789.5 · 682.1 / 799.6 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 10k | SmoothSine | Time | 210 / 217 · 209 / 217 · 211 / 216 · 211 / 214 · 209 / 218 | 482.8 / 496.9 · 480.8 / 496.9 · 484.8 / 494.9 · 484.8 / 490.9 · 480.8 / 498.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 10k | SmoothSine | SampleIndex | 211 / 215 · 209 / 217 · 209 / 214 · 209 / 216 · 210 / 216 | 483.8 / 492.9 · 480.8 / 496.9 · 480.8 / 490.9 · 480.8 / 494.9 · 482.8 / 494.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 10k | BroadbandNoise | Time | 214 / 219 · 212 / 219 · 212 / 218 · 213 / 218 · 214 / 222 | 994.8 / 1004.9 · 990.8 / 1004.9 · 990.8 / 1002.9 · 992.8 / 1002.9 · 994.8 / 1010.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 10k | BroadbandNoise | SampleIndex | 216 / 220 · 213 / 219 · 214 / 222 · 213 / 221 · 214 / 222 | 998.8 / 1006.9 · 992.8 / 1004.9 · 994.8 / 1010.9 · 992.8 / 1008.9 · 993.8 / 1010.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 10k | SpikesAndNaN | Time | 282 / 341 · 282 / 340 · 281 / 343 · 282 / 342 · 280 / 344 | 1378.8 / 1804.7 · 1378.8 / 1802.7 · 1376.8 / 1808.7 · 1377.8 / 1806.7 · 1374.8 / 1810.8 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 10k | SpikesAndNaN | SampleIndex | 283 / 346 · 282 / 342 · 280 / 341 · 281 / 342 · 281 / 346 | 1380.8 / 1814.8 · 1378.8 / 1806.7 · 1374.8 / 1804.7 · 1376.8 / 1806.7 · 1376.8 / 1814.8 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 100k | SmoothSine | Time | 216 / 220 · 213 / 218 · 213 / 219 · 213 / 220 · 214 / 222 | 998.8 / 1006.9 · 992.8 / 1002.9 · 992.8 / 1004.9 · 992.8 / 1006.9 · 994.8 / 1010.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 100k | SmoothSine | SampleIndex | 213 / 219 · 213 / 217 · 214 / 221 · 213 / 220 · 213 / 222 | 992.8 / 1004.9 · 992.8 / 1000.8 · 994.8 / 1008.9 · 992.8 / 1006.9 · 992.8 / 1010.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 100k | BroadbandNoise | Time | 215 / 221 · 214 / 221 · 212 / 221 · 212 / 219 · 214 / 222 | 995.8 / 1008.9 · 994.8 / 1008.9 · 990.8 / 1008.9 · 990.8 / 1004.9 · 993.8 / 1010.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 100k | BroadbandNoise | SampleIndex | 214 / 219 · 212 / 219 · 213 / 217 · 212 / 217 · 214 / 220 | 994.8 / 1004.9 · 990.8 / 1004.9 · 992.8 / 1000.8 · 990.8 / 1000.8 · 994.8 / 1006.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 100k | SpikesAndNaN | Time | 283 / 341 · 281 / 340 · 281 / 342 · 281 / 341 · 283 / 346 | 1380.8 / 1804.7 · 1376.8 / 1802.7 · 1375.8 / 1806.7 · 1376.8 / 1804.7 · 1380.8 / 1814.8 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 100k | SpikesAndNaN | SampleIndex | 282 / 344 · 281 / 346 · 280 / 340 · 281 / 340 · 283 / 342 | 1378.8 / 1810.8 · 1375.8 / 1814.8 · 1374.8 / 1802.7 · 1375.8 / 1802.7 · 1379.8 / 1806.7 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 1M | SmoothSine | Time | 215 / 218 · 213 / 217 · 215 / 222 · 214 / 218 · 214 / 222 | 996.8 / 1002.9 · 992.8 / 1000.8 · 996.8 / 1010.9 · 993.8 / 1002.9 · 994.8 / 1010.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 1M | SmoothSine | SampleIndex | 213 / 218 · 213 / 219 · 213 / 220 · 213 / 219 · 215 / 222 | 992.8 / 1002.9 · 991.8 / 1004.9 · 992.8 / 1006.9 · 992.8 / 1004.9 · 996.8 / 1010.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 1M | BroadbandNoise | Time | 214 / 222 · 213 / 219 · 213 / 219 · 214 / 218 · 214 / 218 | 994.8 / 1010.9 · 992.8 / 1004.9 · 992.8 / 1004.9 · 993.8 / 1002.9 · 994.8 / 1002.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 1M | BroadbandNoise | SampleIndex | 214 / 219 · 212 / 218 · 212 / 218 · 214 / 218 · 213 / 218 | 994.8 / 1004.9 · 990.8 / 1002.9 · 990.8 / 1002.9 · 993.8 / 1002.9 · 992.8 / 1002.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 1M | SpikesAndNaN | Time | 362 / 424 · 361 / 420 · 362 / 423 · 361 / 425 · 363 / 421 | 2657.8 / 3092.8 · 2656.8 / 3084.7 · 2657.8 / 3090.7 · 2656.8 / 3094.8 · 2660.8 / 3086.7 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 1M | SpikesAndNaN | SampleIndex | 362 / 422 · 363 / 423 · 360 / 422 · 361 / 424 · 364 / 429 | 2658.8 / 3088.7 · 2660.8 / 3090.7 · 2654.8 / 3088.7 · 2656.8 / 3092.8 · 2661.8 / 3102.8 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 4M | SmoothSine | Time | 214 / 219 · 213 / 218 · 212 / 219 · 214 / 221 · 216 / 222 | 994.8 / 1004.9 · 992.8 / 1002.9 · 990.8 / 1004.9 · 993.8 / 1008.9 · 998.8 / 1010.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 4M | SmoothSine | SampleIndex | 216 / 221 · 213 / 219 · 212 / 217 · 213 / 219 · 214 / 220 | 998.8 / 1008.9 · 992.8 / 1004.9 · 990.8 / 1000.8 · 992.8 / 1004.9 · 994.8 / 1006.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 4M | BroadbandNoise | Time | 214 / 220 · 213 / 219 · 211 / 216 · 213 / 219 · 214 / 221 | 994.8 / 1006.9 · 992.8 / 1004.9 · 988.7 / 998.8 · 992.8 / 1004.9 · 994.8 / 1008.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 4M | BroadbandNoise | SampleIndex | 213 / 219 · 213 / 218 · 213 / 218 · 212 / 217 · 213 / 219 | 992.8 / 1004.9 · 992.8 / 1002.9 · 992.8 / 1002.9 · 990.8 / 1000.8 · 992.8 / 1004.9 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 4M | SpikesAndNaN | Time | 363 / 425 · 362 / 422 · 361 / 425 · 361 / 421 · 362 / 424 | 2660.8 / 3094.8 · 2658.8 / 3088.7 · 2656.8 / 3094.8 · 2656.8 / 3086.7 · 2658.8 / 3092.8 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |
| 4 | 4M | SpikesAndNaN | SampleIndex | 362 / 423 · 361 / 422 · 361 / 420 · 360 / 422 · 361 / 424 | 2657.8 / 3090.7 · 2656.8 / 3088.7 · 2656.8 / 3084.7 · 2654.8 / 3088.7 · 2655.8 / 3092.8 | 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 · 0 / 0 |

#### Paint with streaming, SampleIndex and Time interleaved (both charts receive 1/60 s of data and are rendered alternately in the same frame; 5 warm-up + 30 measured frames, ms)

| Channels | Samples / ch | Signal | New samples / ch / frame | SampleIndex render median, runs 1–5 | SampleIndex render median, range | SampleIndex render p95, runs 1–5 | SampleIndex render p95, range | Time render median, runs 1–5 | Time render median, range | Time render p95, runs 1–5 | Time render p95, range | p95 ratio SampleIndex / Time, runs 1–5 | p95 ratio SampleIndex / Time, range |
| ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 10k | SmoothSine | 17 | 2.19 · 2.26 · 2.38 · 2.39 · 2.31 | 2.19–2.39 | 2.37 · 2.88 · 3.07 · 2.99 · 2.76 | 2.37–3.07 | 2.20 · 2.19 · 2.35 · 2.38 · 2.30 | 2.19–2.38 | 2.35 · 2.83 · 2.90 · 2.92 · 3.26 | 2.35–3.26 | 1.01 · 1.02 · 1.06 · 1.02 · 0.85 | 0.85–1.06 |
| 1 | 10k | BroadbandNoise | 17 | 2.41 · 2.40 · 2.48 · 2.56 · 2.60 | 2.40–2.60 | 2.48 · 2.52 · 2.67 · 2.83 · 5.03 | 2.48–5.03 | 2.41 · 2.37 · 2.47 · 2.52 · 2.66 | 2.37–2.66 | 2.57 · 2.47 · 2.66 · 2.68 · 4.90 | 2.47–4.90 | 0.97 · 1.02 · 1.01 · 1.05 · 1.03 | 0.97–1.05 |
| 1 | 10k | SpikesAndNaN | 17 | 2.32 · 2.40 · 2.42 · 2.53 · 3.32 | 2.32–3.32 | 2.47 · 2.60 · 2.59 · 2.86 · 5.06 | 2.47–5.06 | 2.32 · 2.35 · 2.39 · 2.52 · 3.40 | 2.32–3.40 | 2.62 · 2.98 · 2.61 · 2.72 · 5.58 | 2.61–5.58 | 0.94 · 0.87 · 0.99 · 1.05 · 0.91 | 0.87–1.05 |
| 1 | 100k | SmoothSine | 167 | 2.15 · 2.34 · 2.28 · 2.40 · 3.24 | 2.15–3.24 | 2.41 · 2.60 · 2.41 · 2.53 · 4.70 | 2.41–4.70 | 2.17 · 2.30 · 2.25 · 2.41 · 3.12 | 2.17–3.12 | 2.33 · 2.54 · 2.64 · 3.07 · 5.01 | 2.33–5.01 | 1.04 · 1.02 · 0.91 · 0.82 · 0.94 | 0.82–1.04 |
| 1 | 100k | BroadbandNoise | 167 | 2.90 · 2.88 · 2.89 · 3.08 · 3.57 | 2.88–3.57 | 3.18 · 2.99 · 3.08 · 3.28 · 6.44 | 2.99–6.44 | 2.85 · 2.87 · 2.90 · 3.11 · 3.89 | 2.85–3.89 | 3.58 · 3.06 · 3.08 · 3.47 · 4.82 | 3.06–4.82 | 0.89 · 0.98 · 1.00 · 0.94 · 1.34 | 0.89–1.34 |
| 1 | 100k | SpikesAndNaN | 167 | 2.33 · 2.44 · 2.34 · 2.53 · 3.28 | 2.33–3.28 | 2.63 · 2.68 · 2.49 · 3.07 · 6.21 | 2.49–6.21 | 2.30 · 2.41 · 2.35 · 2.58 · 3.17 | 2.30–3.17 | 2.98 · 2.76 · 2.53 · 2.86 · 5.84 | 2.53–5.84 | 0.88 · 0.97 · 0.98 · 1.07 · 1.06 | 0.88–1.07 |
| 1 | 1M | SmoothSine | 1667 | 2.18 · 2.29 · 2.23 · 2.37 · 2.70 | 2.18–2.70 | 2.36 · 2.57 · 2.41 · 2.74 · 4.72 | 2.36–4.72 | 2.16 · 2.32 · 2.19 · 2.31 · 2.72 | 2.16–2.72 | 2.27 · 2.44 · 2.65 · 2.62 · 4.56 | 2.27–4.56 | 1.04 · 1.05 · 0.91 · 1.04 · 1.04 | 0.91–1.05 |
| 1 | 1M | BroadbandNoise | 1667 | 3.04 · 3.16 · 3.17 · 3.24 · 3.83 | 3.04–3.83 | 3.45 · 3.34 · 3.56 · 3.59 · 5.32 | 3.34–5.32 | 3.07 · 3.20 · 3.15 · 3.21 · 3.54 | 3.07–3.54 | 3.37 · 3.37 · 3.30 · 3.43 · 4.36 | 3.30–4.36 | 1.02 · 0.99 · 1.08 · 1.05 · 1.22 | 0.99–1.22 |
| 1 | 1M | SpikesAndNaN | 1667 | 2.28 · 2.40 · 2.27 · 2.43 · 2.48 | 2.27–2.48 | 2.40 · 2.67 · 2.54 · 2.58 · 4.44 | 2.40–4.44 | 2.25 · 2.40 · 2.26 · 2.41 · 2.47 | 2.25–2.47 | 2.72 · 2.55 · 2.41 · 2.62 · 3.81 | 2.41–3.81 | 0.88 · 1.05 · 1.05 · 0.98 · 1.16 | 0.88–1.16 |
| 1 | 4M | SmoothSine | 6667 | 2.26 · 2.31 · 2.26 · 2.39 · 2.33 | 2.26–2.39 | 2.51 · 2.45 · 3.16 · 2.95 · 3.13 | 2.45–3.16 | 2.23 · 2.29 · 2.24 · 2.41 · 2.34 | 2.23–2.41 | 2.59 · 2.46 · 2.45 · 2.64 · 3.37 | 2.45–3.37 | 0.97 · 1.00 · 1.29 · 1.12 · 0.93 | 0.93–1.29 |
| 1 | 4M | BroadbandNoise | 6667 | 3.18 · 3.27 · 3.32 · 3.36 · 3.23 | 3.18–3.36 | 3.76 · 3.52 · 3.52 · 3.67 · 5.58 | 3.52–5.58 | 3.16 · 3.29 · 3.21 · 3.34 · 3.26 | 3.16–3.34 | 3.90 · 3.40 · 3.59 · 3.76 · 4.93 | 3.40–4.93 | 0.96 · 1.03 · 0.98 · 0.98 · 1.13 | 0.96–1.13 |
| 1 | 4M | SpikesAndNaN | 6667 | 2.32 · 2.49 · 2.41 · 2.44 · 2.34 | 2.32–2.49 | 2.75 · 2.85 · 2.79 · 4.03 · 2.67 | 2.67–4.03 | 2.30 · 2.49 · 2.42 · 2.42 · 2.33 | 2.30–2.49 | 2.52 · 2.76 · 2.97 · 3.86 · 2.85 | 2.52–3.86 | 1.09 · 1.03 · 0.94 · 1.04 · 0.94 | 0.94–1.09 |
| 4 | 10k | SmoothSine | 17 | 6.43 · 6.88 · 12.03 · 7.73 · 6.70 | 6.43–12.03 | 7.19 · 8.78 · 14.11 · 10.45 · 9.76 | 7.19–14.11 | 6.48 · 6.63 · 12.31 · 7.43 · 7.02 | 6.48–12.31 | 7.12 · 10.43 · 13.04 · 9.22 · 9.52 | 7.12–13.04 | 1.01 · 0.84 · 1.08 · 1.13 · 1.03 | 0.84–1.13 |
| 4 | 10k | BroadbandNoise | 17 | 7.47 · 7.64 · 14.50 · 10.87 · 9.00 | 7.47–14.50 | 7.73 · 8.61 · 20.78 · 16.00 · 15.12 | 7.73–20.78 | 7.39 · 7.59 · 14.64 · 10.92 · 10.58 | 7.39–14.64 | 8.52 · 8.19 · 15.68 · 15.21 · 15.27 | 8.19–15.68 | 0.91 · 1.05 · 1.33 · 1.05 · 0.99 | 0.91–1.33 |
| 4 | 10k | SpikesAndNaN | 17 | 6.99 · 7.27 · 8.87 · 13.69 · 7.03 | 6.99–13.69 | 8.32 · 7.91 · 14.53 · 14.68 · 13.04 | 7.91–14.68 | 7.03 · 7.07 · 7.61 · 13.03 · 7.02 | 7.02–13.03 | 7.54 · 7.67 · 13.90 · 15.24 · 9.22 | 7.54–15.24 | 1.10 · 1.03 · 1.05 · 0.96 · 1.41 | 0.96–1.41 |
| 4 | 100k | SmoothSine | 167 | 6.48 · 6.80 · 7.07 · 7.89 · 6.71 | 6.48–7.89 | 7.91 · 8.10 · 10.45 · 9.72 · 9.40 | 7.91–10.45 | 6.34 · 6.78 · 7.02 · 7.82 · 7.04 | 6.34–7.82 | 7.32 · 10.49 · 8.51 · 9.74 · 9.83 | 7.32–10.49 | 1.08 · 0.77 · 1.23 · 1.00 · 0.96 | 0.77–1.23 |
| 4 | 100k | BroadbandNoise | 167 | 9.42 · 9.96 · 19.23 · 19.31 · 11.38 | 9.42–19.31 | 11.51 · 18.56 · 24.13 · 22.39 · 18.71 | 11.51–24.13 | 9.31 · 10.42 · 19.26 · 18.49 · 12.74 | 9.31–19.26 | 9.58 · 19.22 · 21.13 · 20.77 · 19.27 | 9.58–21.13 | 1.20 · 0.97 · 1.14 · 1.08 · 0.97 | 0.97–1.20 |
| 4 | 100k | SpikesAndNaN | 167 | 6.98 · 7.50 · 13.44 · 9.95 · 7.39 | 6.98–13.44 | 7.82 · 11.97 · 14.58 · 13.30 · 16.20 | 7.82–16.20 | 6.91 · 7.48 · 13.23 · 8.92 · 7.49 | 6.91–13.23 | 8.45 · 14.05 · 16.31 · 11.67 · 18.46 | 8.45–18.46 | 0.93 · 0.85 · 0.89 · 1.14 · 0.88 | 0.85–1.14 |
| 4 | 1M | SmoothSine | 1667 | 6.31 · 7.08 · 7.15 · 8.85 · 8.36 | 6.31–8.85 | 7.11 · 10.36 · 13.21 · 11.58 · 15.44 | 7.11–15.44 | 6.32 · 7.08 · 7.47 · 9.86 · 8.24 | 6.32–9.86 | 7.53 · 7.82 · 12.24 · 12.19 · 14.52 | 7.53–14.52 | 0.94 · 1.32 · 1.08 · 0.95 · 1.06 | 0.94–1.32 |
| 4 | 1M | BroadbandNoise | 1667 | 10.05 · 10.79 · 20.70 · 11.67 · 11.70 | 10.05–20.70 | 11.82 · 13.40 · 28.98 · 20.43 · 15.87 | 11.82–28.98 | 10.07 · 11.30 · 20.69 · 11.50 · 10.96 | 10.07–20.69 | 12.14 · 15.77 · 32.13 · 19.20 · 18.13 | 12.14–32.13 | 0.97 · 0.85 · 0.90 · 1.06 · 0.88 | 0.85–1.06 |
| 4 | 1M | SpikesAndNaN | 1667 | 6.96 · 8.02 · 12.50 · 7.44 · 6.82 | 6.82–12.50 | 8.53 · 11.31 · 13.51 · 14.36 · 10.69 | 8.53–14.36 | 6.97 · 8.13 · 12.30 · 7.51 · 6.88 | 6.88–12.30 | 11.31 · 12.38 · 18.42 · 11.59 · 8.07 | 8.07–18.42 | 0.75 · 0.91 · 0.73 · 1.24 · 1.32 | 0.73–1.32 |
| 4 | 4M | SmoothSine | 6667 | 6.75 · 7.87 · 10.26 · 7.00 · 8.34 | 6.75–10.26 | 18.32 · 11.28 · 12.97 · 9.72 · 17.48 | 9.72–18.32 | 6.76 · 7.28 · 9.49 · 6.77 · 8.24 | 6.76–9.49 | 18.43 · 10.83 · 12.94 · 8.20 · 15.16 | 8.20–18.43 | 0.99 · 1.04 · 1.00 · 1.18 · 1.15 | 0.99–1.18 |
| 4 | 4M | BroadbandNoise | 6667 | 15.86 · 17.88 · 21.69 · 21.18 · 10.86 | 10.86–21.69 | 19.27 · 21.77 · 26.15 · 22.71 · 13.30 | 13.30–26.15 | 16.19 · 18.48 · 21.05 · 19.75 · 10.97 | 10.97–21.05 | 20.30 · 22.18 · 23.47 · 23.65 · 16.59 | 16.59–23.65 | 0.95 · 0.98 · 1.11 · 0.96 · 0.80 | 0.80–1.11 |
| 4 | 4M | SpikesAndNaN | 6667 | 7.79 · 10.48 · 13.04 · 7.91 · 7.04 | 7.04–13.04 | 9.79 · 14.05 · 15.06 · 12.58 · 8.99 | 8.99–15.06 | 7.68 · 10.74 · 13.06 · 8.49 · 6.99 | 6.99–13.06 | 10.30 · 13.87 · 13.86 · 13.40 · 11.50 | 10.30–13.87 | 0.95 · 1.01 · 1.09 · 0.94 · 0.78 | 0.78–1.09 |

#### First paint after resize (cold path)

| Case | Work per trial | Trials | Min (ms), runs 1–5 | Median (ms), runs 1–5 | Median (ms), range | p95 (ms), runs 1–5 | p95 (ms), range | Median per call (µs), runs 1–5 | Median per call (µs), range |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 ch × 1M BroadbandNoise, Time; render at 1000×800, resize to 1974×1080, time the next render | 1 render | 10 | 29.59 · 30.12 · 66.09 · 57.09 · 28.82 | 34.63 · 64.83 · 67.39 · 69.37 · 35.41 | 34.63–69.37 | 43.63 · 72.03 · 73.80 · 71.38 · 60.22 | 43.63–73.80 | – · – · – · – · – | – |

#### Insertion

| Case | Work per trial | Trials | Min (ms), runs 1–5 | Median (ms), runs 1–5 | Median (ms), range | p95 (ms), runs 1–5 | p95 (ms), range | Median per call (µs), runs 1–5 | Median per call (µs), range |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `addSample`, autoscale ON, 1M samples in buffer | 100000 calls | 10 | 1.09 · 3.21 · 3.23 · 2.84 · 2.93 | 1.11 · 4.46 · 3.24 · 3.22 · 3.20 | 1.11–4.46 | 1.14 · 5.52 · 3.54 · 3.25 · 3.22 | 1.14–5.52 | 0.0111 · 0.0446 · 0.0324 · 0.0322 · 0.0320 | 0.0111–0.0446 |
| `addSample`, autoscale OFF, 1M samples in buffer | 100000 calls | 10 | 1.65 · 1.12 · 3.00 · 1.10 · 1.22 | 1.68 · 2.18 · 3.23 · 1.44 · 2.15 | 1.44–3.23 | 2.00 · 3.22 · 3.26 · 2.22 · 3.22 | 2.00–3.26 | 0.0168 · 0.0218 · 0.0323 · 0.0144 · 0.0215 | 0.0144–0.0323 |
| `addUniformSamples`, one 1M-sample block into an empty channel | 1 call (1M samples) | 10 | 4.03 · 3.73 · 8.64 · 3.85 · 3.69 | 4.97 · 4.35 · 8.93 · 4.59 · 3.85 | 3.85–8.93 | 8.92 · 6.43 · 9.39 · 8.27 · 8.29 | 6.43–9.39 | – · – · – · – · – | – |
| `addDataPoint` (legacy), SampleIndex, autoscale OFF, empty channel | 100000 calls | 10 | 1.36 · 1.95 · 3.12 · 1.15 · 1.09 | 1.55 · 3.12 · 3.22 · 1.20 · 1.26 | 1.20–3.22 | 1.67 · 6.67 · 3.28 · 3.44 · 2.08 | 1.67–6.67 | 0.0155 · 0.0312 · 0.0322 · 0.0120 · 0.0126 | 0.0120–0.0322 |

<!-- bench_StripChart results: end -->

---

## Historical: profile of the 4 ch × 1M case (#5)

> **Historical.** From the 2.1.1 session (two runs at `75bb7d0`), before #73 changed the drawing of decimated traces;
> the hot path it shows (`drawPolyline`) is no longer the one used.

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

## Baseline: 2.0.1, re-measured in the same session (historical)

> **Historical.** This comparison was made in the 2.1.1 session (two runs of `bench_StripChart` at `75bb7d0`,
> before the rendering changes of #11–#16 and #73). Its 2.1.1 column is run 1 of that session, which is no longer on
> this page; it is not re-measured by the session above.

Measured in the same session, on the same machine, with the benchmark at commit `e5595b2` compiled against the
library at tag `v2.0.1` (commit `f2fdcb7`), same compiler and `Release` flags. That benchmark version still used the
**previous, index-based signal generator**; at `N` = 1M, the only sample count used below, its samples are
bit-identical to those of the current generator, so the two columns draw the same data. The 2.1.1 column is run 1
of that session, with the range of its two runs in brackets.

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

The single source of performance figures for `QtIndustrialWidgets::AnalysisChart` (#25). The figures come from the
session of the StripChart part above: **5 runs** of `bench_AnalysisChart` (benchmark source commit `7d72487`),
alternated with the runs of `bench_StripChart`, merged with `benchmarks/merge_runs.py`, every run published.
`docs/analysis_chart.md`, README and CHANGELOG quote this part.

## Target

#25: a redraw after a zoom or pan step takes **≤ 16.7 ms** for **4 channels × 10 M samples at a 1920 device px
plot**. A case passes when the **p95** of its 30 measured redraws is ≤ 16.7 ms; it is marked PASS on this page only
if it passes in all 5 runs.

**Result: not met.** 6 of the 7 cases FAIL. Panning at 1/100 of the record (100 k samples per channel in the view)
passes in all 5 runs (p95 at most 14.64 ms); panning at 1/10 of the record passes in 1 of the 5 runs. The full view
of the four 10 M-sample channels has a median of 26.67 · 34.50 · 39.66 · 46.43 · 33.53 ms (`SpikesAndNaN`).

## Command

See *Commands* in the StripChart part: the same build and the same session.

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

Merged with `benchmarks/merge_runs.py` from 5 runs (run 1: analysis_run1.md, run 2: analysis_run2.md, run 3: analysis_run3.md, run 4: analysis_run4.md, run 5: analysis_run5.md).

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
| Benchmark source commit | 7d72487c5be53e1dc5eb365275c813268bea7c24 |

#### Redraw after a zoom or pan step (target ≤ 16.7 ms at p95)

| Case | Samples per channel in view | Decimated channels | min (ms), runs 1–5 | median (ms), runs 1–5 | median (ms), range | p95 (ms), runs 1–5 | p95 (ms), range | Target, runs 1–5 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Full view, redraw only (SpikesAndNaN) | 1e+07 | 4 / 4 | 20.29 · 22.79 · 27.59 · 29.37 · 21.88 | 26.67 · 34.50 · 39.66 · 46.43 · 33.53 | 26.67–46.43 | 35.06 · 44.63 · 47.23 · 49.92 · 39.62 | 35.06–49.92 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Zoom in, wheel x1.25 per step | 1e+07 → 1.238e+04 | 4 / 4 | 4.51 · 4.73 · 6.14 · 8.18 · 4.32 | 9.72 · 10.26 · 14.41 · 15.42 · 8.15 | 8.15–15.42 | 21.60 · 30.06 · 41.45 · 46.85 · 24.72 | 21.60–46.85 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Zoom out, wheel x1.25 per step | 1.238e+04 → 1e+07 | 4 / 4 | 4.31 · 4.64 · 7.34 · 10.14 · 4.63 | 7.88 · 9.02 · 14.48 · 14.90 · 11.54 | 7.88–14.90 | 30.44 · 29.19 · 38.25 · 48.44 · 35.18 | 29.19–48.44 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Pan, arrow keys 10 % per step, 1/10 of the record | 1e+06 | 4 / 4 | 9.58 · 8.77 · 17.17 · 10.67 · 9.23 | 12.63 · 10.83 · 17.69 · 20.16 · 14.96 | 10.83–20.16 | 19.05 · 14.62 · 20.52 · 23.60 · 18.09 | 14.62–23.60 | FAIL · PASS · FAIL · FAIL · FAIL |
| Pan, arrow keys 10 % per step, 1/100 of the record | 1e+05 | 4 / 4 | 4.77 · 4.65 · 10.75 · 5.44 · 5.71 | 5.84 · 4.91 · 11.18 · 12.23 · 10.80 | 4.91–12.23 | 11.00 · 8.01 · 14.64 · 14.52 · 11.90 | 8.01–14.64 | PASS · PASS · PASS · PASS · PASS |
| Full view, redraw only (SmoothSine) | 1e+07 | 4 / 4 | 36.39 · 29.88 · 42.35 · 58.94 · 34.37 | 57.46 · 35.90 · 59.76 · 69.23 · 54.95 | 35.90–69.23 | 65.16 · 46.84 · 68.86 · 73.67 · 62.32 | 46.84–73.67 | FAIL · FAIL · FAIL · FAIL · FAIL |
| Full view, redraw only (BroadbandNoise) | 1e+07 | 4 / 4 | 36.70 · 32.62 · 48.54 · 52.20 · 36.14 | 59.31 · 40.34 · 68.73 · 74.25 · 51.89 | 40.34–74.25 | 69.12 · 70.11 · 77.34 · 81.11 · 64.89 | 64.89–81.11 | FAIL · FAIL · FAIL · FAIL · FAIL |

#### setChannelData()

| Item | Value |
| --- | --- |
| Time per call (4 calls of 10000000 samples) | run 1: min 101.26 ms, median 102.52 ms; run 2: min 105.79 ms, median 118.85 ms; run 3: min 127.15 ms, median 147.24 ms; run 4: min 184.40 ms, median 189.43 ms; run 5: min 101.45 ms, median 120.61 ms |
| Stored samples (8 B timestamp + 8 B value) | 610.4 MiB |
| Pyramid | 81.1 MiB (2.13 B/sample, 13.3 % of the samples) |
| Resident set growth across the 4 calls | run 1: 691.9 MiB; run 2: 691.8 MiB; run 3: 691.9 MiB; run 4: 691.9 MiB; run 5: 691.9 MiB |

<!-- bench_AnalysisChart results: end -->

## Cause of the miss (profile, historical)

> **Historical.** Measured during the development of #25, before #73 changed the drawing of decimated traces (one
> vertical stroke per column); the phase times are those of the polyline drawing.

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

> **Historical.** A before/after comparison of the #73 drawing change, measured in its own session; the CHANGELOG
> entry of #73 quotes it. The current figures are in the *Results* section of this part.

The drawing of decimated traces changed in #73: each device column is drawn as one vertical line from its minimum
to its maximum, plus the connection to the next column, instead of the polyline first → min → max → last, which
strokes the column's extent twice. This section measures that change against the drawing before it, in **one
session** on the machine of the StripChart part.

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
