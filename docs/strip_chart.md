<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->

# StripChart: High-Rate Real-Time Telemetry and M4 Decimation

`QtIndustrialWidgets::StripChart` is a real-time multi-channel scrolling oscilloscope and strip chart recorder designed for industrial test benches, data acquisition (DAQ) systems, SCADA telemetry, and embedded instrumentation.

In QtIndustrialWidgets 2.1.0, `StripChart` features a true nanosecond time axis, high-throughput bulk insertion APIs, and pixel-column M4 decimation for data streams of up to millions of points per channel. Measured paint and insertion times, with machine and method, are in the [Benchmark Report](benchmarks.md).

---

## 1. Real-Time Time Axis

In earlier versions, `StripChart` used sample buffer index as its horizontal axis. In version 2.0.0+, `StripChart` supports dual horizontal axis modes:

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

### Switching Axis Mode
Switching `xAxisMode()` between `SampleIndex` and `Time` clears all buffered sample data across all channels (ring contents, sample counts, latest values, timestamps, and decimation states). This prevents mixing incompatible timebases (sample index vs nanoseconds). Channel configuration metadata (name, color, pen width, visibility) and the diagnostic `rejectedSampleCount()` are preserved.

When using explicit-timestamp APIs (`addSample`, `addSamples`, `addUniformSamples`, `addSynchronousSamples`) while in `SampleIndex` mode, timestamps are ignored and samples are stored using the channel's running sample index (`totalSamples`), preventing channel poisoning. A warning is logged on the first occurrence per channel.

---

## 2. M4 Pixel-Column Decimation

Rendering raw waveforms with millions of data points every frame is CPU-bound and does not scale. `StripChart` integrates an internal M4 decimation engine based on the Jugel et al. M4 algorithm. In the measured streaming cases the number of heap allocations per frame does not depend on the number of samples for continuous signals; with NaN gaps it grows with the number of visible gaps (see the [Benchmark Report](benchmarks.md)).

### How It Works
The plot is `W_dev = round(W_logical * devicePixelRatioF())` device pixels wide, and every decimation bucket corresponds to exactly one device column (see "Bucket-to-column rule" below). For each bucket:
1. Samples falling into the bucket are aggregated into four extremal vertices:
   - **First** in time `(t_first, y_first)`
   - **Minimum** value `(t_min, y_min)`
   - **Maximum** value `(t_max, y_max)`
   - **Last** in time `(t_last, y_last)`
2. Vertices are emitted in strict **chronological order** (First, then Min/Max in occurrence order, then Last) with coincident points deduplicated.
3. The total emitted vertices per channel are bounded by `≤ 4 · W_dev` points, rendered as contiguous `QPolygonF` polylines.

### Critical Guarantees and Limitations
- **Peaks are never lost (except in columns that contain a non-finite sample, see "Sensor dropouts" in section 4); intra-pixel waveform shape is not preserved**: Because the global minimum and maximum within every pixel column are explicitly captured and emitted, transient spikes, glitches, and faults are guaranteed to be drawn regardless of decimation factor. However, decimation does not eliminate acquisition Nyquist aliasing present in the underlying telemetry, nor does it retain intra-pixel waveform shapes.
- **Trace Shimmer Prevention**: Bucket indices are anchored to absolute time (`k = floor(t · W_dev / span)`) or to the absolute sample index (`k = floor(i · W_dev / capacity)`), and the visible window always starts and ends on bucket boundaries. Advancing the window by less than one bucket moves no sample to another column; advancing it by one bucket shifts the decimated trace by exactly one device column.

### Bucket-to-column rule
- **One bucket per device column**: the window holds exactly `W_dev` buckets, and bucket `k` occupies device column `k − kStart` of the plot, in both axis modes and at any device pixel ratio, including fractional ones such as 1.25 and 1.5.
- **Exact arithmetic**: bucket assignment uses integers only (`k = floor(x · W_dev / den)` with `den` = time span in ns or buffer capacity, computed with a portable 128-bit multiply/divide). Bucket widths are therefore `floor` or `ceil` of `den / W_dev` and need not be equal; in Time mode the window covers exactly the configured time span for spans of at least `W_dev` ns; shorter spans use 1 ns buckets and the window is `W_dev` ns wide (time labels describe that window).
- **Device-pixel snapping**: the plot's left edge is snapped to a device pixel boundary using the painter's device transform, so the rule also holds when the widget itself sits at a fractional device position (common with fractional scaling). With a rotated or sheared painter the snapping is skipped.
- **Vertex placement**: all vertices of a decimated bucket are drawn at the centre of its device column, so its min/max is a single crisp vertical line and a single-sample spike lights exactly one column. Because an aliased cosmetic polyline can leave out its first or last pixel, the end pixels of every decimated segment are drawn explicitly; a segment made of a single bucket is therefore always visible.
- **Steep edges**: with decimation, a steep edge between two adjacent buckets (from the last sample of one column to the first sample of the next) is drawn across both columns: the connection runs from one column centre to the next, so each column shows the half of the edge nearest to it. A raw trace draws the same edge in a single column.
- **Raw traces** use the same window and a continuous position in it, so at the `DecimationMode::Auto` switch a sample moves by at most half a device pixel.
- **SampleIndex window**: the window holds the `W_dev` newest buckets. When the buffer has wrapped, the oldest, partial bucket (fewer than `capacity / W_dev` samples) can fall out on the left, and with a full buffer the newest sample lies less than one device pixel from the right edge.

### Decimation Modes
- `DecimationMode::Auto`: Automatically activates M4 decimation when the visible sample count exceeds `W_dev` (plot width in device pixels); otherwise renders raw samples (default).
- `DecimationMode::Always`: Forces M4 decimation regardless of sample count.
- `DecimationMode::Off`: Renders raw samples directly.

### Cosmetic 1-Device-Pixel Trace Rendering
To guarantee strictly bounded frame times ($O(W)$) even when decimating dense high-frequency noise:
- **Thin Decimated Traces Rule**: When decimation is active (either via `Always` or triggered by `Auto`), waveforms are drawn with a cosmetic 1-device-pixel pen without antialiasing (`Qt::FlatCap`, `Qt::MiterJoin`; the end pixels of each segment are drawn explicitly, see "Bucket-to-column rule"). In dense M4 min/max zig-zags (up to ~7,680 vertices on a 1920-px plot), antialiased wide pens force `QPainter`'s rasterizer (`QStroker`) to compute outline polygons for every zig-zag segment, which is expensive for dense data. Drawing with a cosmetic 1-device-pixel unaliased pen avoids `QStroker` outline synthesis entirely. Measured paint times are in the [Benchmark Report](benchmarks.md).
- **Raw Traces**: When decimation is inactive (via `Off` or when sample count $\le W_{dev}$), user-configured channel pen widths and antialiasing remain active for smooth visual appearance.
- **Head Glow Dot**: Telemetry head glow dots always retain antialiasing for clean visual indicators.

**Visible thickness switch in `Auto` mode (expected behaviour)**: Because of the rule above, a channel in `DecimationMode::Auto` changes appearance at runtime. While its visible sample count is `≤ W_dev` it is drawn raw, with its `penWidth` (default 1.8 px, set via `addChannel()`) and antialiasing; as soon as the visible sample count exceeds `W_dev` (typically while the buffer fills the time window) it switches to the 1-device-pixel unaliased pen, so the trace becomes visibly thinner. The decision is taken per channel on every frame, so the trace becomes thick again if the visible sample count drops back to `≤ W_dev` (for example after widening the plot or shortening the visible window). To avoid the switch, set `DecimationMode::Always` for a consistent look at all fill levels, or keep the channel `penWidth` close to 1 so both paths look alike.

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
- **Lazy Autoscale with Hysteresis**: Ingestion functions mark autoscale dirty in $O(1)$. Recomputation occurs once per frame inside `paintEvent` (or on programmatic calls to `yMinimum()` / `yMaximum()`) by scanning only the visible window ($O(W)$). Dynamic hysteresis expands range immediately on spikes and delays range shrinking until 30 consecutive frames with data span < 50% of the range, preventing cached grid redraw churn.

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

### Sensor dropouts
A non-finite sample is how a sensor dropout reaches the chart, and a dropout must never be hidden: a trace that looks continuous across a failed sensor is misleading.
- **Decimated traces**: a pixel column (decimation bucket) that contains any non-finite sample is left **blank**, and the trace is broken on both sides. The finite samples of that column, including peaks, are not drawn. Adjacent dropout columns form one contiguous blank region. This applies in both axis modes, while streaming (a dropout in the newest column blanks it on the next frame) and after buffer eviction (a column is drawn again once its non-finite samples have been evicted).
- **Glow dot**: it is not drawn when the newest column is blank (the legend keeps showing the latest value), and it never draws inside a blank column.
- **Raw traces**: the polyline is broken at the non-finite sample. With the default 1.8 px antialiased pen and about one sample per device pixel, the ends of the two polylines can cover the dropout's column, so a fully blank column is not guaranteed in raw mode.
- **Autoscale** still uses all finite values, including those of a blank column.
- **Column alignment**: a blank bucket is always exactly one device column, at any position and device pixel ratio (see "Bucket-to-column rule" in section 2).

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

### Axis Mode Switching (2.0.1)
In version 2.0.1, switching `xAxisMode()` explicitly clears all channel sample buffers to prevent timestamp poisoning. If your application dynamically toggles modes, ensure that acquisition buffers are re-streamed or restarted after changing modes.

