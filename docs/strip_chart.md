<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->

# StripChart: High-Rate Real-Time Telemetry and M4 Decimation

`QtIndustrialWidgets::StripChart` is a real-time multi-channel scrolling oscilloscope and strip chart recorder designed for industrial test benches, data acquisition (DAQ) systems, SCADA telemetry, and embedded instrumentation.

In QtIndustrialWidgets 2.0.0, `StripChart` features a true nanosecond time axis, high-throughput bulk insertion APIs, and pixel-column M4 decimation capable of displaying data streams sampled at hundreds of kHz (millions of points per channel) at 60 FPS.

---

## 1. Real-Time Time Axis

In earlier versions, `StripChart` used sample buffer index as its horizontal axis. In version 2.0.0, `StripChart` supports dual horizontal axis modes:

- `XAxisMode::SampleIndex`: Legacy mode where points are positioned by index in the circular buffer.
- `XAxisMode::Time`: Nanosecond-precision real-time axis based on `std::chrono::nanoseconds`.

### Time Representation
All timestamps in the public API use `std::chrono::nanoseconds` (64-bit integer), eliminating floating-point jitter and ambiguities between seconds, milliseconds, or microseconds:

```cpp
auto *chart = new QtIndustrialWidgets::StripChart(parent);
chart->setXAxisMode(QtIndustrialWidgets::StripChart::XAxisMode::Time);
chart->setTimeSpan(std::chrono::seconds(10)); // 10-second scrolling window

int ch1 = chart->addChannel("Torque", Qt::cyan, 1.8);
chart->addSample(ch1, std::chrono::nanoseconds(1000000000), 45.2);
```

### Time Origin and Label Formats
- **Relative Format (`TimeLabelFormat::Relative`)**: Displays time relative to the leading edge (e.g., `-10 s ... 0 s`). Labels automatically scale units (ns, µs, ms, s, min) and live in the cached background reticle.
- **Absolute Format (`TimeLabelFormat::Absolute`)**: Maps timestamps to wall-clock time via `setTimeOrigin(const QDateTime &origin)`. Dynamic labels update in real time (`HH:mm:ss.zzz`).

### 1-2-5 Tick Selection Rule
Vertical time grid lines adapt dynamically to the visible span using the standard 1-2-5 decade progression (1, 2, 5, 10, 20, 50, etc.), ensuring intuitive intervals.

---

## 2. M4 Pixel-Column Decimation

Rendering raw waveforms with millions of data points every frame causes severe CPU/GPU bottlenecking. `StripChart` integrates an internal, zero-allocation M4 decimation engine based on the Jugel et al. M4 algorithm.

### How It Works
For each device pixel column `k` (`W_dev = round(W_logical * devicePixelRatioF())`):
1. Samples falling into bucket `k` are aggregated into four extremal vertices:
   - **First** in time `(t_first, y_first)`
   - **Minimum** value `(t_min, y_min)`
   - **Maximum** value `(t_max, y_max)`
   - **Last** in time `(t_last, y_last)`
2. Vertices are emitted in strict **chronological order** (First, then Min/Max in occurrence order, then Last) with coincident points deduplicated.
3. The total emitted vertices per channel are bounded by `≤ 4 · W_dev` points, rendered as contiguous `QPolygonF` polylines.

### Critical Guarantees and Limitations
- **Peaks are never lost**: Because the global minimum and maximum within every pixel column are explicitly captured and emitted, transient spikes, glitches, and faults are guaranteed to be drawn regardless of decimation factor.
- **Anti-aliasing note**: Decimation preserves signal extremes, but does **not** eliminate acquisition Nyquist aliasing present in the underlying telemetry, nor does it retain intra-pixel waveform shapes.
- **Trace Shimmer Prevention**: Bucket indices are anchored to absolute time (`k = floor(t / Δt_px)`), and the visible window boundary `t_end` is quantized to the next bucket boundary (`ceil(t_latest / Δt_px) * Δt_px`). This guarantees zero pixel shimmer during scrolling.

### Decimation Modes
- `DecimationMode::Auto`: Automatically activates M4 decimation when the visible sample count exceeds `2 · W_dev`; otherwise renders raw samples (default).
- `DecimationMode::Always`: Forces M4 decimation regardless of sample count.
- `DecimationMode::Off`: Renders raw samples directly.

---

## 3. High-Rate Sample Insertion API

All insertion functions must be called from the **GUI thread**:

```cpp
// Single sample
void addSample(int channelId, std::chrono::nanoseconds t, double value);

// Batch of arbitrary timestamps
void addSamples(int channelId, const std::chrono::nanoseconds *t,
                const double *values, qsizetype count);

// Uniformly sampled block (e.g. 100 kHz DAQ chunk)
void addUniformSamples(int channelId, std::chrono::nanoseconds t0,
                       std::chrono::nanoseconds dt,
                       const double *values, qsizetype count);

// Synchronous multi-channel acquisition
void addSynchronousSamples(std::chrono::nanoseconds t,
                           const QVector<double> &values);
```

### Rules & Diagnostics
- **Monotonic Timestamps**: Timestamps per channel must be non-decreasing (`t >= lastTimestamp`). Out-of-order samples are rejected, counted in `rejectedSampleCount(channelId)`, and logged with a rate-limited warning.
- **Arrival Time vs Acquisition Time**: Calling legacy `addDataPoint(channelId, value)` while in `Time` mode stamps samples with the internal monotonic clock (`QElapsedTimer`). This represents **GUI arrival time** (subject to event loop latency) rather than sensor acquisition time.
- **Bulk Emission Efficiency**: Bulk insertion methods emit `dataAdded()` once per call and recalculate autoscaling lazily at most once per frame.

---

## 4. Gap Detection and Non-Finite Telemetry Handling

### Gaps
Polylines break automatically into separate segments when:
1. An explicit gap occurs where `Δt > gapThreshold`.
2. Automatic gap detection is enabled (`gapThreshold = 0` ns): the threshold is set dynamically to `4 * EMA(Δt)` (Exponential Moving Average of inter-sample intervals).
3. Empty buckets are never drawn as zero and never bridged unless within the gap threshold.

### NaN and ±Inf Handling
- NaN and ±∞ values are **stored** in the circular buffer as sensor fault telemetry.
- They are excluded from autoscale range calculation and filtered from `QPainter` polylines.
- The legend overlay displays `---` when the latest telemetry value is non-finite.

---

## 5. Buffer Memory Sizing

Each channel stores circular Structure-of-Arrays (SOA) buffers:
- `std::vector<qint64> timestamps` (8 bytes/sample)
- `std::vector<double> values` (8 bytes/sample)
- **Total cost**: 16 bytes per sample per channel.

Capacity can be configured up to 2^24 (16,777,216 samples per channel = 256 MB per channel):

```cpp
chart->setCapacity(1000000); // 1 million points = 16 MB per channel
```

If the circular buffer is full and cannot cover the configured `timeSpan()`, `isTimeWindowFullyCovered()` returns `false` and a warning suggests increasing capacity.

---

## 6. Migration Guide (Version 1.x to 2.0.0)

In version 2.0.0, the public `channel(int)` method and `ChannelInfo` struct were removed to encapsulate internal storage.

Replace previous calls as follows:

| Version 1.x Call | Version 2.0.0 Replacement |
|:---|:---|
| `chart->channel(id)->name` | `chart->channelName(id)` |
| `chart->channel(id)->color` | `chart->channelColor(id)` |
| `chart->channel(id)->visible` | `chart->isChannelVisible(id)` |
| `chart->channel(id)->penWidth` | `chart->channelPenWidth(id)` |
| `chart->channel(id)->count` | `chart->channelSampleCount(id)` |
| `chart->channel(id)->latestValue` | `chart->channelLatestValue(id)` |
