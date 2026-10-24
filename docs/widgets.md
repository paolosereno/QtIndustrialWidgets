<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->

# Widgets {#widgets}

One section per widget: what it shows, its main properties and how it is driven. All classes live in namespace
`QtIndustrialWidgets` (short alias `qiw`). The images are the gallery showcases (`--export-gallery`, dark theme, see
[gallery_images.md](gallery_images.md)). The colour properties of each widget are listed in [theming.md](theming.md);
the complete API is in the class documentation.

## RadialGauge

<img src="images/radial-gauge-warning.png" alt="RadialGauge" width="300">

Circular gauge for speed, pressure, temperature and similar values (`qiw::RadialGauge`).
- **Sweep**: `startAngle` and `spanAngle` set the arc, for example 270°, 240° or 180°.
- **Scale**: `majorTicks` and `minorTicks`, with numeric labels at the major ticks.
- **Thresholds**: `warningThreshold` and `errorThreshold`, shown as coloured arc bands (`thresholdBandsVisible`);
  `warningExceeded(bool)` and `errorExceeded(bool)` report crossings.
- **Digital display**: optional (`digitalDisplayVisible`), with `precision` and `unit`; it is sized and placed clear
  of the scale labels.
- **Rendering**: the static scale is cached in a pixmap and rebuilt on geometry or appearance changes; a value change
  redraws the needle and the display.

## LinearGauge

<img src="images/linear-gauge-thermometer.png" alt="LinearGauge" width="120">

Bar gauge and thermometer (`qiw::LinearGauge`).
- **Orientation**: `Qt::Vertical` or `Qt::Horizontal`.
- **Thermometer mode** (`thermometerMode`): a bulb at the bottom (vertical) or on the left (horizontal); otherwise a
  plain bar.
- **Liquid colour**: fixed (`liquidColor`), following the warning and error thresholds (`dynamicLiquidColor`), or a
  gradient fill (`gradientLiquid`).
- **Scale and display**: major and minor ticks with labels (`scaleVisible`), optional digital display with `precision`
  and `unit`.

## SevenSegmentDisplay

<img src="images/seven-segment-display-value.png" alt="SevenSegmentDisplay" width="300">

Vector seven-segment display for numeric readouts (`qiw::SevenSegmentDisplay`).
- **Vector segments**: the segments are painted as polygons and scale with the widget.
- **Format**: `digitCount`, `decimalPlaces`, `showLeadingZeros`, `showDecimalPoint`; a value that rounds to zero is
  shown without a minus sign.
- **Shape**: `skewAngle` (italic slant) and `segmentWidthRatio` (segment thickness).
- **Colours**: `activeSegmentColor` for lit segments and `inactiveSegmentColor` for unlit ones.
- **Text** (`setText()`): digits, `-`, `_`, `=`, space and the letter patterns A, b, C, c, d, E, F, H, L, o, P, r, U.
  Upper- and lower-case input give the same pattern, except `C`/`c` (two patterns) and `r` (lower case only); any
  other character is shown blank.

## LedIndicator

<img src="images/led-indicator-on.png" alt="LedIndicator" width="140">

LED indicator lamp (`qiw::LedIndicator`).
- **Shape**: `LedShape::Circular` or `LedShape::Rectangular`, with an optional bezel (`bezelVisible`) and glow
  (`glowEffect`).
- **State**: `on`, with `toggle()`, and blinking (`blinking`) at `blinkRateMs`.
- **Label**: `labelText`, drawn in `textColor` (or the palette's window text colour when `textColor` is not set).
- **Interaction**: optional (`clickable`), emitting `clicked()`.

## IndustrialKnob

<img src="images/industrial-knob-continuous.png" alt="IndustrialKnob" width="220">

Rotary knob (`qiw::IndustrialKnob`).
- **Modes**: `KnobMode::Continuous` (any value in the range, with `step`) or `KnobMode::Discrete` (`discreteSteps`
  positions, for selectors).
- **Scale**: arc set by `startAngle` and `spanAngle`, major and minor ticks with labels, an active track
  (`trackVisible`) and a value display (`valueDisplayVisible`).
- **Input**: mouse drag (the value follows the pointer's angle), mouse wheel, and the keys arrows, PageUp/PageDown,
  Home/End.
- **Rendering**: the knob has a serrated grip of 32 teeth; ticks and scale are cached, so a rotation redraws only the
  moving parts.

## StripChart

<img src="images/strip-chart-channels.png" alt="StripChart" width="640">

Live, scrolling multi-channel chart for acquisition data (`qiw::StripChart`). Guide: [strip_chart.md](strip_chart.md).
- **Time axis**: `std::chrono::nanoseconds` timestamps, 1-2-5 tick steps, labels relative (`-10 s … 0 s`) or absolute
  wall-clock time (`HH:mm:ss.zzz`); a sample-index axis is also available.
- **M4 decimation**: with more samples in the window than device pixel columns, each column draws the first, minimum,
  maximum and last sample of its bucket (at most 4 vertices per column per channel), with a cosmetic 1-device-pixel pen
  without antialiasing. Buckets are anchored to absolute time (or sample index), so they keep their contents while the
  window scrolls. Peaks are kept, except in a column that holds a non-finite sample: that column is left blank so a
  dropout stays visible.
- **Gaps and dropouts**: a non-finite value breaks the trace, and so does a time gap larger than `gapThreshold`
  (default: 4 × an exponential moving average of the sampling interval).
- **Autoscale**: optional; the range grows at once to include new values and shrinks only after 30 consecutive
  recomputations in which the data spans less than half of it.
- **Channels**: independent channels with name, colour, pen width and visibility, and a legend (`legendVisible`).
- **Performance**: measured paint and insertion times, with the targets and their results, are in the
  [Benchmark Report](benchmarks.md).

## IndustrialSwitch

<img src="images/industrial-switch-guarded-toggle.png" alt="IndustrialSwitch" width="180">

Toggle or rocker switch (`qiw::IndustrialSwitch`).
- **Types**: `SwitchType::ToggleLever` or `SwitchType::Rocker`.
- **Positions**: 2 (`labelOff` / `labelOn`) or 3 (`labelOff` / `labelCenter` / `labelOn`), via `positionCount`.
- **Safety guard**: optional cover (`hasSafetyGuard`) that has to be opened (`guardOpen`, `guardToggled(bool)`)
  before the switch can be operated.
- **Orientation**: `Qt::Vertical` or `Qt::Horizontal`.
- **Details**: optional status LED (`hasLed`), animated movement (`animated`), mouse click and drag, and the keys
  Space, Return and the arrows.

## LevelMeter

<img src="images/level-meter-stereo-db.png" alt="LevelMeter" width="170">

Multi-channel level meter (`qiw::LevelMeter`).
- **Channels**: any number (`channelCount`), with labels (`setChannelLabels()`).
- **Display**: `DisplayMode::Segmented` (LED segments, `segmentCount`) or `DisplayMode::Continuous` (bar).
- **Zones**: normal, warning and error, set by `warningThreshold` and `errorThreshold`; `overloadOccurred(int)` reports
  a channel entering the error zone.
- **Peak hold**: optional (`peakHoldEnabled`), held for `peakHoldTimeMs`, then falling at `peakDecayRate` units per
  second.
- **Orientation and scale**: vertical or horizontal, optional scale (`scaleVisible`) with a `unit` such as dB.

## AnnunciatorPanel

<img src="images/annunciator-panel-alarms.png" alt="AnnunciatorPanel" width="640">

Alarm annunciator grid following the ISA-18.1 sequences A and M (`qiw::AnnunciatorPanel`).
- **Grid**: `rows` × `columns` tiles, each with a text and a severity (`Severity::Critical`, `Warning` or `Advisory`).
- **Sequences**: `SequenceA_AutomaticReset` and `SequenceM_ManualReset`, with the states `Normal`, `Unacknowledged`
  (fast flash), `Acknowledged` (steady) and `Ringback` (slow flash).
- **Operator actions**: `acknowledge()` / `acknowledgeAll()`, `silence()`, `reset()` / `resetAll()` and
  `setLampTest()`; `audibleHornChanged(bool)` reports the horn state. Clicking an unacknowledged tile acknowledges it
  (`tileClicked(int)`).

## Compass

<img src="images/compass-heading-up.png" alt="Compass" width="300">

Heading indicator (`qiw::Compass`).
- **Modes**: `DisplayMode::HeadingUp` (the card rotates with the heading) or `DisplayMode::NorthUp` (fixed card,
  rotating needle).
- **Heading bug**: target heading (`targetHeading`), optionally set with the mouse (`headingBugInteractive`); the
  difference from the heading is `courseDeviation`.
- **Card**: cardinal (N, E, S, W) and intercardinal (NE, SE, SW, NW) labels and three-digit degree numbers.
- **References**: lubber line (`lubberLineVisible`) and digital heading readout (`digitalReadoutVisible`).

## CircularBargraph

<img src="images/circular-bargraph-valid.png" alt="CircularBargraph" width="260">

Bargraph of curved segments on an arc (`qiw::CircularBargraph`).
- **Modes**: `DisplayMode::Bar` (segments lit up to the value) or `DisplayMode::Dot` (one segment).
- **Geometry**: arc set by `startAngle` and `spanAngle` (e.g. 180°, 270° or 360°), `segmentCount` segments.
- **Threshold zones by position**: each segment takes the colour of the zone (normal, warning, error) containing its
  centre; unlit segments show a dimmed tint of their zone over `ghostColor`. The setters keep
  `warningThreshold <= errorThreshold`.
- **Peak hold**: optional, held for `peakHoldTimeMs`, then decaying at `peakDecayRate`.
- **Digital display**: optional, with `precision` and `unit`.
- **Value state**: `valueState()` reports `Valid`, `Underrange`, `Overrange` or `Invalid`. Out-of-range inputs
  (including ±Inf) are shown at the range bound with the end segment blinking and a `<` / `>` readout; a NaN input is
  shown as `Invalid` (all segments unlit, readout `---`). `rawValue()` keeps the unclamped input.

## AnalysisChart

<img src="images/analysis-chart-review.png" alt="AnalysisChart" width="640">

Offline review of recorded multi-channel data (`qiw::AnalysisChart`). Guide: [analysis_chart.md](analysis_chart.md).
- **Recorded data**: per-channel copies of absolute `std::chrono::nanoseconds` timestamps and `double` values, at
  different or irregular rates; decreasing timestamps are rejected and the previous data kept.
- **Zoom and pan**: wheel zoom around the pointer (Ctrl+wheel for values), drag to pan, box zoom, keyboard navigation,
  double-click or Home to reset.
- **Cursors**: two cursors with nearest-sample readouts (no interpolation), Δt and Δy, in a readout band of up to 6
  channels that is part of the minimum size.
- **Same rules as StripChart**: non-finite values and time gaps break the trace, and M4 decimation draws one bucket
  per device column with dropout columns left blank; a min/max pyramid built by `setChannelData()` (at most 2.125
  bytes per sample on top of the 16 of the data) supplies the buckets.
- **Performance**: the target of #25 (redraw after a zoom or pan step ≤ 16.7 ms for 4 channels × 10 M samples at
  1920 px) is not met; tables, method and cause in the
  [Benchmark Report](benchmarks.md#analysischart-performance-benchmarks).
