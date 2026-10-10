<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->

# Changelog

All notable changes to **QtIndustrialWidgets** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
