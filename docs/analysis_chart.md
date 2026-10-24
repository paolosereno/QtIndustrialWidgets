<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->

# AnalysisChart: Offline Review of Recorded Data

`QtIndustrialWidgets::AnalysisChart` shows recorded multi-channel data for offline review: zoom, pan, two
measurement cursors and a readout band. It complements `StripChart`, which stays the live, streaming display.
Both charts apply the same rules to dropouts, gaps and decimation (section 5); the decimation buckets of
`AnalysisChart` follow the view, those of `StripChart` follow its scrolling window.

```cpp
#include <QtIndustrialWidgets/AnalysisChart.h>

auto *chart = new qiw::AnalysisChart(parent);
const int pressure = chart->addChannel("Pressure", Qt::cyan);
chart->setChannelData(pressure, timestamps.data(), values.data(), static_cast<qsizetype>(values.size()));
chart->setCursorVisible(qiw::AnalysisChart::Cursor::A, true);
chart->setCursorVisible(qiw::AnalysisChart::Cursor::B, true);
```

---

## 1. Data

- **Channels** have stable ids: `addChannel()` returns an id that is never reused and is not affected by
  removing other channels. Ids increase in the order the channels are added.
- **`setChannelData(id, t, values, count)`** copies `count` absolute `std::chrono::nanoseconds` timestamps and
  `double` values; the caller's buffers can be freed after the call. Each channel has its own timestamps
  (different or irregular rates are allowed).
- Timestamps must be **non-decreasing**. Otherwise the call returns `false`, issues a warning and keeps the
  previous data. `count == 0` clears the channel.
- **`clear()`** empties the data of every channel and keeps the channels; **`removeAllChannels()`** removes the
  channels.
- All channels share **one Y axis**.

## 2. View

- `setXRange()`, `setYRange()` and `resetView()` set the view from code; `viewChanged()` reports every change.
- The time span stays between a few samples of the densest visible channel (4 × its mean sample interval, never
  below 1 ns) and the full data extent, and the view may leave the data by at most one view width.
- `resetView()` shows the full extent, with Y covering the finite values of the visible channels plus a 10 %
  margin (StripChart's autoscale margin). The first data loaded sets the view this way.
- Time labels: `TimeLabelFormat::Relative` shows the time elapsed since the data start; `Absolute` shows
  `timeOrigin() + t` as wall-clock time (`HH:mm:ss.zzz`), or `t` itself when `timeOrigin()` is invalid.

## 3. Interaction

The widget takes keyboard focus (`Qt::StrongFocus`).

| Input | Action |
|---|---|
| Wheel | Zoom X around the pointer, ×1.25 per notch |
| Ctrl + wheel | Zoom Y around the pointer |
| Left drag | Pan X and Y; a drag that starts within 4 logical px of a visible cursor moves that cursor |
| Right drag, or Shift + left drag | Box zoom (a box under 4 logical px is ignored) |
| Double-click, Home | `resetView()` |
| `+` / `-` | Zoom X around the centre |
| ← / → | Pan X by 10 % of the view |
| ↑ / ↓ | Pan Y by 10 % of the view |

## 4. Cursors and readout band

- Two vertical cursors, `Cursor::A` and `Cursor::B`, are hidden until `setCursorVisible()`. When the first data
  is loaded they are placed at 1/3 and 2/3 of the view. `setCursorPosition()` moves them from code; they can also
  be dragged.
- **`cursorReadout(channel, cursor)`** returns the sample nearest in time to the cursor (no interpolation; on a
  tie, the earlier one). It returns no value when the cursor is hidden or lies outside that channel's data.
- **`cursorDeltaTime()`** is t_B − t_A of the cursor positions; **`cursorDeltaValue(channel)`** is y_B − y_A,
  with no value when either readout is missing or non-finite.
- `cursorsChanged()` reports a cursor moved, shown or hidden, or new data under it.

The **readout band** below the time axis shows the cursor times and Δt, then for each listed channel y_A, y_B
and Δy. `---` marks a missing or non-finite readout.

- **Units and decimals.** The time unit and the decimals of the band depend on the **view**, not on the widget
  size. Times use the unit of a nominal 10-tick time axis over the view, to a tenth of its step. Values have one
  decimal more than a nominal 10-tick value axis. So the band does not change when the widget is resized, and
  `minimumSizeHint()` always holds it whole; on a widget much narrower or wider than that nominal axis, the
  band's unit or decimals can differ from those of the axis labels (for example the axis in ms, the band in s).
- **At most 6 channels.** The band lists the first 6 visible channels, in id order (the order they were added);
  further visible channels are counted in a last row, `+N more`. Their readouts are available through
  `cursorReadout()` and `cursorDeltaValue()`, and `setChannelVisible()` chooses which channels the band lists.
- The band is part of `minimumSizeHint()`: from that size up no text of the band is clipped.
- `readoutVisible` hides the band; `cursorColor` sets the colour of the cursor lines.

## 5. Rendering and data quality

- **Dropouts.** A non-finite value (NaN, ±Inf) breaks the trace. In a decimated trace the device column of a
  bucket that holds a non-finite sample stays blank, as in `StripChart` (#11).
- **Gaps.** A time step larger than `gapThreshold()` breaks the trace. With the default 0 the threshold is
  4 × an exponential moving average of the sampling interval of the channel (weights 0.9 / 0.1, in sample order),
  as in `StripChart`.
- **Decimation** (`decimationMode`, same meaning as in `StripChart`): with `Auto` (the default) a channel with
  more samples in the view than device columns of the plot is drawn M4-decimated, one bucket per device column:
  each column shows the first, minimum, maximum and last finite sample of its bucket, with a cosmetic
  1-device-pixel pen and without antialiasing. `Off` always draws the raw samples, `Always` always decimates.
  Buckets are `floor((t − xStart) · W_dev / span)`, so they follow the view; the plot's edges are snapped to
  device pixels, and traces, ticks and cursors share one device mapping at every device pixel ratio.
- **Min/max pyramid.** `setChannelData()` builds a pyramid over the values (blocks of 64 samples, then levels
  that each combine two nodes of the level below), so a redraw combines whole blocks instead of reading every
  sample in the view. Its buckets equal those of `M4Decimator::decimateToBuckets()` on the same samples.

## 6. Memory

Per sample, a channel stores:

| Part | Bytes per sample |
|---|---|
| Timestamp and value (copied by `setChannelData()`) | 16 |
| Pyramid: one 64 B node per block of 64 samples, levels above it adding at most as much again, and the first timestamp of each block (8 B per 64 samples) | ≤ 2.125 |

The pyramid adds at most 2.125 / 16 ≈ 13.3 % to the stored samples (the node size is 64 B on 64-bit
platforms). For 4 channels × 10 M samples that is 640 MB of samples plus at most 85 MB of pyramid. The measured
figures, with the time of `setChannelData()`, are in the [Benchmark Report](benchmarks.md#analysischart-performance-benchmarks).

## 7. Performance

Target of #25: a redraw after a zoom or pan step takes at most 16.7 ms for 4 channels × 10 M samples at a
1920 device px plot. **It is not met** on the machine of the
[Benchmark Report](benchmarks.md#analysischart-performance-benchmarks) (one session of 5 runs): 6 of its 7 cases
FAIL. The full view of the four 10 M-sample channels redraws with a median of 26.67–46.43 ms over the 5 runs with
the `SpikesAndNaN` signal; panning at 1/100 of the record (100 k samples per channel in the view) passes in all 5
runs, with a p95 of at most 14.64 ms. The report has the full tables, the method and the earlier profiles (drawing
the decimated envelope, then reading the buckets from the pyramid).

## 8. Not covered (later phases)

Getting data out of a `StripChart`, loading files, several Y axes, horizontal cursors, statistics between the
cursors, FFT, export and printing.
