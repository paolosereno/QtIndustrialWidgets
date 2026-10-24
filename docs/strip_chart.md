<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->

# StripChart: time axis, M4 decimation and insertion

`QtIndustrialWidgets::StripChart` is a multi-channel scrolling chart for live data: samples are appended as they arrive, and the chart shows the most recent time span (or the most recent samples).

It has a time axis in nanoseconds, insertion functions for single samples and for blocks, and M4 decimation to one bucket per device pixel column, with buffers of up to 16,777,216 samples per channel. Measured paint and insertion times, with machine and method, are in the [Benchmark Report](benchmarks.md).

---

## 1. Time axis

`StripChart` has two horizontal axis modes:

- `XAxisMode::SampleIndex`: points are positioned by their index in the circular buffer.
- `XAxisMode::Time`: points are positioned by their timestamp, a `std::chrono::nanoseconds`.

### Time Representation
All timestamps in the public API are `std::chrono::nanoseconds` (a 64-bit integer count), so timestamps are not rounded to floating point and their unit is not ambiguous:

```cpp
auto *chart = new QtIndustrialWidgets::StripChart(parent);
chart->setXAxisMode(QtIndustrialWidgets::StripChart::XAxisMode::Time);
chart->setTimeSpan(std::chrono::seconds(10)); // 10-second scrolling window

int ch1 = chart->addChannel("Torque", Qt::cyan, 1.8);
chart->addSample(ch1, std::chrono::nanoseconds(1000000000), 45.2);
```

### Time Origin and Label Formats
- **Relative Format (`TimeLabelFormat::Relative`)**: Displays time relative to the leading edge (e.g., `-10 s ... 0 s`). The label unit (ns, µs, ms, s, min) follows the visible span.
- **Absolute Format (`TimeLabelFormat::Absolute`)**: Maps timestamps to wall-clock time via `setTimeOrigin(const QDateTime &origin)`. The labels use `HH:mm:ss.zzz` for spans shorter than 10 s and `HH:mm:ss` otherwise.

### 1-2-5 Tick Selection Rule
The spacing of the vertical time grid lines follows the 1-2-5 progression (1, 2, 5, 10, 20, 50, …) and is chosen from the visible span.

### Switching Axis Mode
Switching `xAxisMode()` between `SampleIndex` and `Time` clears all buffered sample data across all channels (ring contents, sample counts, latest values, timestamps, and decimation states). This prevents mixing incompatible timebases (sample index vs nanoseconds). Channel configuration metadata (name, color, pen width, visibility) and the diagnostic `rejectedSampleCount()` are preserved.

When using explicit-timestamp APIs (`addSample`, `addSamples`, `addUniformSamples`, `addSynchronousSamples`) while in `SampleIndex` mode, timestamps are ignored and samples are stored using the channel's running sample index (`totalSamples`). A warning is logged on the first occurrence per channel.

---

## 2. M4 pixel-column decimation

Drawing every sample of a long buffer on every frame takes time in proportion to the number of samples. `StripChart` reduces the samples of each device pixel column to at most four vertices with the M4 algorithm (Jugel et al.). In the streaming cases of the [Benchmark Report](benchmarks.md) one channel needs about 125 heap allocations per frame at every buffer size from 10k to 4M samples (`SmoothSine`, `BroadbandNoise`), and about 140–165 with NaN dropouts (`SpikesAndNaN`).

### How It Works
The plot is `W_dev = round(W_logical * devicePixelRatioF())` device pixels wide, and every decimation bucket corresponds to exactly one device column (see "Bucket-to-column rule" below). For each bucket:
1. Samples falling into the bucket are aggregated into four extremal vertices:
   - **First** in time `(t_first, y_first)`
   - **Minimum** value `(t_min, y_min)`
   - **Maximum** value `(t_max, y_max)`
   - **Last** in time `(t_last, y_last)`
2. Vertices are emitted in strict **chronological order** (First, then Min/Max in occurrence order, then Last) with coincident points deduplicated.
3. The total emitted vertices per channel are bounded by `≤ 4 · W_dev` points. Each device column is drawn as one vertical line from its minimum to its maximum vertex, plus the connection from its last vertex to the first vertex of the next column (#73); each column's extent is stroked once.

### What decimation keeps and what it does not
- **Peaks are never lost (except in columns that contain a non-finite sample, see "Sensor dropouts" in section 4); intra-pixel waveform shape is not preserved**: Because the global minimum and maximum within every pixel column are explicitly captured and emitted, transient spikes, glitches and faults are drawn at any decimation factor. However, decimation does not eliminate acquisition Nyquist aliasing present in the underlying telemetry, nor does it retain intra-pixel waveform shapes.
- **No shimmer while scrolling**: Bucket indices are anchored to absolute time (`k = floor(t · W_dev / span)`) or to the absolute sample index (`k = floor(i · W_dev / capacity)`), and the visible window always starts and ends on bucket boundaries. Advancing the window by less than one bucket moves no sample to another column; advancing it by one bucket shifts the decimated trace by exactly one device column.

### Bucket-to-column rule
- **One bucket per device column**: the window holds exactly `W_dev` buckets, and bucket `k` occupies device column `k − kStart` of the plot, in both axis modes and at any device pixel ratio, including fractional ones such as 1.25 and 1.5.
- **Exact arithmetic**: bucket assignment uses integers only (`k = floor(x · W_dev / den)` with `den` = time span in ns or buffer capacity, computed with a portable 128-bit multiply/divide). Bucket widths are therefore `floor` or `ceil` of `den / W_dev` and need not be equal; in Time mode the window covers exactly the configured time span for spans of at least `W_dev` ns; shorter spans use 1 ns buckets and the window is `W_dev` ns wide (time labels describe that window).
- **Device-pixel snapping**: the plot's left edge is snapped to a device pixel boundary using the painter's device transform, so the rule also holds when the widget itself sits at a fractional device position (common with fractional scaling). With a rotated or sheared painter the snapping is skipped.
- **Vertex placement**: all vertices of a decimated bucket are drawn at the centre of its device column, so its min/max is a single vertical line and a single-sample spike lights exactly one column. Because an aliased cosmetic line can leave out its end pixel, the end pixels of the lines of every column and of every decimated segment are drawn explicitly; a segment made of a single bucket is therefore always visible. Compared with the polyline drawn before #73 (first → min → max → last), a column can light one more row at either end of its extent (#73).
- **Steep edges**: with decimation, a steep edge between two adjacent buckets (from the last sample of one column to the first sample of the next) is drawn across both columns: the connection runs from one column centre to the next, so each column shows the half of the edge nearest to it. A raw trace draws the same edge in a single column.
- **Raw traces** use the same window and a continuous position in it, so at the `DecimationMode::Auto` switch a sample moves by at most half a device pixel.
- **SampleIndex window**: the window holds the `W_dev` newest buckets. When the buffer has wrapped, the oldest, partial bucket (fewer than `capacity / W_dev` samples) can fall out on the left, and with a full buffer the newest sample lies less than one device pixel from the right edge.

### Decimation modes
- `DecimationMode::Auto`: Automatically activates M4 decimation when the visible sample count exceeds `W_dev` (plot width in device pixels); otherwise renders raw samples (default).
- `DecimationMode::Always`: Forces M4 decimation regardless of sample count.
- `DecimationMode::Off`: Renders raw samples directly.

### Pens of decimated and raw traces
A decimated channel has at most `4 · W_dev` vertices per frame, however many samples it shows. The traces are drawn as follows:
- **Decimated traces**: When decimation is active (either via `Always` or triggered by `Auto`), waveforms are drawn with a cosmetic 1-device-pixel pen without antialiasing (`Qt::FlatCap`, `Qt::MiterJoin`; the end pixels of each segment are drawn explicitly, see "Bucket-to-column rule"). A 1920-pixel plot can have up to 7,680 such vertices per channel. Measured paint times are in the [Benchmark Report](benchmarks.md).
- **Raw traces**: when decimation is inactive (`Off`, or a visible sample count `≤ W_dev` in `Auto`), the channel's pen width and antialiasing are used.
- **Glow dot**: the dot at the newest sample is always drawn with antialiasing.

**Visible thickness switch in `Auto` mode (expected behaviour)**: Because of the rule above, a channel in `DecimationMode::Auto` changes appearance at runtime. While its visible sample count is `≤ W_dev` it is drawn raw, with its `penWidth` (default 1.8 px, set via `addChannel()`) and antialiasing; as soon as the visible sample count exceeds `W_dev` (typically while the buffer fills the time window) it switches to the 1-device-pixel unaliased pen, so the trace becomes visibly thinner. The decision is taken per channel on every frame, so the trace becomes thick again if the visible sample count drops back to `≤ W_dev` (for example after widening the plot or shortening the visible window). To avoid the switch, set `DecimationMode::Always` for a consistent look at all fill levels, or keep the channel `penWidth` close to 1 so both paths look alike.

---

## 3. Insertion functions

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

### Rules and diagnostics
- **Monotonic timestamps**: timestamps per channel must be non-decreasing (`t >= lastTimestamp`). Out-of-order samples are rejected, counted in `rejectedSampleCount(channelId)`, and logged with a warning at the first rejection and at every 1000th.
- **Arrival time and acquisition time**: calling `addDataPoint(channelId, value)` while in `Time` mode stamps samples with the internal monotonic clock (`QElapsedTimer`). This represents **GUI arrival time** (subject to event loop latency) rather than sensor acquisition time.
- **Autoscale**: the insertion functions only mark the Y range as out of date. It is recomputed at most once per frame, in `paintEvent` (or when `yMinimum()` / `yMaximum()` is called), from the samples in the visible window. The range grows at once when the data exceed it, and shrinks only after 30 consecutive recomputations in which the data span is less than 50 % of the range.

---

## 4. Gaps and non-finite values

### Gaps
Polylines break automatically into separate segments when:
1. An explicit gap occurs where `Δt > gapThreshold`.
2. Automatic gap detection is enabled (`gapThreshold = 0` ns): the threshold is set dynamically to `4 * EMA(Δt)` (Exponential Moving Average of inter-sample intervals).
3. Empty buckets are never drawn as zero and never bridged unless within the gap threshold.

### NaN and ±Inf
- NaN and ±∞ values are **stored** in the circular buffer, as a record of sensor faults.
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

## 5. Buffer memory

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

## 6. Migrating from version 1.x to 2.0.0

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

### Axis mode switching (2.0.1)
In version 2.0.1, switching `xAxisMode()` explicitly clears all channel sample buffers to prevent timestamp poisoning. If your application dynamically toggles modes, ensure that acquisition buffers are re-streamed or restarted after changing modes.

