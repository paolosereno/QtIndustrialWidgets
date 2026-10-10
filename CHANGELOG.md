<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->

# Changelog

All notable changes to **QtIndustrialWidgets** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [2.1.0] - 2026-10-10

### Changed
- **Cosmetic 1-device-pixel decimated traces**: Decimated waveforms are now rendered with a cosmetic 1-device-pixel pen without antialiasing (`Qt::FlatCap`, `Qt::MiterJoin`). This eliminates `QStroker` outline polygon synthesis across dense M4 zig-zags, reducing polyline rasterization time by over 1,000×.
- **Auto decimation threshold**: `DecimationMode::Auto` threshold adjusted from `W_dev / 2` to `W_dev` (plot area width in device pixels), ensuring raw rendering is used whenever sample count $\le W_{dev}$.
- **`yRangeChanged` emission timing**: Autoscale recomputation is now lazy. Signal `yRangeChanged(double, double)` is no longer emitted synchronously inside sample insertion methods (`addDataPoint`, `addSample`, etc.); it is emitted only during frame paint (`paintEvent`) or explicit programmatic queries (`yMinimum()`, `yMaximum()`).
- **Autoscale hysteresis**: Dynamic Y autoscale now applies hysteresis (`kAutoScaleMargin = 0.10`, `kAutoScaleShrinkThreshold = 0.50`, `kAutoScaleShrinkHysteresisCount = 30`). Range expands immediately on data spikes; range shrinking is deferred until the data span remains below 50% of the current range for 30 consecutive recomputations, completely eliminating cached grid redraw churn.

### Performance
- **$O(W)$ steady-state rendering**: Wired incremental M4 streaming (`M4Decimator::IncrementalStream`) directly into sample ingestion with cached `GeometryKey` validation and circular buffer eviction handling. Reusable scratch buffers in private d-pointer eliminate per-frame heap allocations.
- **Ingestion throughput**: Removed synchronous $O(N)$ full-buffer scanning from all six sample insertion APIs, marking autoscale dirty in $O(1)$.
- **Before / After benchmarks (1920 device px, offscreen)**:
  | Benchmark Case | Baseline (2.0.1) | 2.1.0 Release | Improvement |
  |---|---|---|---|
  | `addSample` (autoscale ON, 1M base) | 365,049 ms (3.65 ms/call) | **1.10 ms (0.011 µs/call)** | **331,862× faster** |
  | `addSample` (autoscale OFF, 1M base) | 3.25 ms (0.033 µs/call) | **1.13 ms (0.011 µs/call)** | **~3× faster** |
  | `addUniformSamples` (1M block) | 7.15 ms | **3.62 ms** | **2× faster** |
  | Offscreen paint 1 ch × 1M noisy | 6,916 ms | **3.40 ms** | **2,034× faster** |
  | Offscreen paint 4 ch × 1M noisy | ~28,000 ms | **23.38 ms** | **~1,200× faster** |
  | First paint after resize (4 ch × 1M) | 21,467 ms | **46.43 ms** | **462× faster** |

### Fixed
- **Documentation claims**: Corrected performance claims to reflect measured benchmark results, clarified that peaks are never lost while intra-pixel waveform shape is not preserved, and documented the 1-device-pixel cosmetic pen design.

---

## [2.0.1] - 2026-10-10

### Fixed
- **SampleIndex decimation dropping recent samples**: Fixed integer floor division and missing bucket coverage in `computeIndexBuckets` that caused the most recent samples (up to 20% of the buffer) to never be drawn in decimated SampleIndex mode.
- **Time mode bucket boundary spike drop**: Fixed `computeTimeWindow` boundary formula where `tLatest == tEnd` excluded the leading sample point landing on bucket boundaries from decimation and window coverage checks.
- **Axis mode switching timestamp incompatibility**: Fixed channel buffer poisoning when switching between `Time` and `SampleIndex` modes or calling explicit-timestamp APIs in `SampleIndex` mode.

### Changed
- **Axis mode reset**: `setXAxisMode()` now clears all channel sample buffers when the mode changes to prevent mixing incompatible timebases, while preserving channel configuration metadata and `rejectedSampleCount()`.
- **Ignored timestamps in SampleIndex mode**: Explicit-timestamp insertion APIs (`addSample()`, `addSamples()`, `addUniformSamples()`, `addSynchronousSamples()`) in `SampleIndex` mode now record samples sequentially using the channel's running sample index (`totalSamples`), ignoring supplied timestamps and logging a one-time diagnostic warning per channel.

---

## [2.0.0] - 2026-10-10

### Breaking Changes & Migration from 1.x
- **Namespace encapsulation**: All widgets are now encapsulated inside `namespace QtIndustrialWidgets` (aliased as `namespace qiw`). The legacy `Q` prefix has been removed from widget class names (`RadialGauge`, `LinearGauge`, `SevenSegmentDisplay`, `LedIndicator`, `IndustrialKnob`, `StripChart`, `IndustrialSwitch`, `LevelMeter`, `AnnunciatorPanel`, `Compass`).
- **Encapsulated Channel Storage**: The `channel(int)` accessor and public `struct ChannelInfo` have been removed from `StripChart`. Use the direct accessors instead:
  - `channelName(int channelId)`
  - `channelColor(int channelId)`
  - `isChannelVisible(int channelId)`
  - `channelPenWidth(int channelId)`
  - `channelSampleCount(int channelId)`
  - `channelLatestValue(int channelId)`
- **Symbol Visibility**: Shared library targets now build with `CXX_VISIBILITY_PRESET hidden` and `VISIBILITY_INLINES_HIDDEN ON` to protect private ABI boundaries.

### Added
- **Nanosecond Real-Time Axis (`StripChart`)**:
  - Full nanosecond precision using `std::chrono::nanoseconds` for acquisition timestamps.
  - Dual axis modes: `XAxisMode::Time` (real-time streaming) and `XAxisMode::SampleIndex` (legacy index-based scrolling).
  - Dynamic 1-2-5 decade tick progression for oscilloscope reticle grid.
  - Relative time labels (`-10 s … 0 s`) with automatic unit scaling (ns, µs, ms, s, min) and Absolute wall-clock time labels (`HH:mm:ss.zzz`) via `setTimeOrigin(const QDateTime &)`.
- **M4 Min/Max Pixel-Column Decimation (`StripChart`)**:
  - Zero-aliasing peak preservation engine ensuring transient spikes, anomalies, and extreme values are never lost at high sampling rates.
  - Bounded vertex generation ($\le 4 \cdot W_{dev}$ vertices per channel) rendered as contiguous polylines.
  - Trace shimmer prevention via window boundary quantization ($t_{end} = \lceil t_{latest} / \Delta t_{px} \rceil \cdot \Delta t_{px}$).
  - Automatic and manual decimation modes (`DecimationMode::Auto`, `DecimationMode::Always`, `DecimationMode::Off`).
- **High-Rate Sample Insertion API (`StripChart`)**:
  - High-throughput insertion methods: `addSample()`, `addSamples()`, `addUniformSamples()`, and `addSynchronousSamples()`.
  - GUI arrival time vs. acquisition time distinction for legacy `addDataPoint()` slot.
  - Strict monotonic timestamp validation with rate-limited diagnostics and `rejectedSampleCount()`.
- **Gap Detection & Fault Telemetry**:
  - Automatic inter-sample gap detection based on $4 \times \text{EMA}(\Delta t)$ moving average or explicit `gapThreshold()`.
  - Raw storage of NaN and $\pm\infty$ sensor dropouts without painter crashes or autoscale disruption.
  - Legend displays `---` when the latest value is non-finite.
- **Buffer Capacity Scaling**: Circular SOA storage clamped up to $2^{24}$ (16,777,216 samples/channel, 16 bytes/sample/channel).
- **Gallery Showcase**: New "⚡ High-Rate DAQ" tab streaming 100 kHz telemetry with synthetic glitches, noise, dropouts, and real-time paint duration benchmarks.
- **Documentation**: New [StripChart Architecture Guide](docs/strip_chart.md).

### Changed
- **Autoscale Optimization**: Lazy evaluation recalculates vertical Y range at most once per frame or synchronously on range read.
- **Bulk Signal Efficiency**: Bulk insertion methods emit `dataAdded()` once per call.

### Fixed
- **Autoscale Infinity Resilience**: Addressed regression where `+Inf` inputs could corrupt vertical scale bounds.
- **Trace Shimmer**: Eliminated pixel jitter while scrolling by anchoring bucket indices to absolute time.
