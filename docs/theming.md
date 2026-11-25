<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->

# Theming and Styling Guide {#theming}

Each widget has its own colour properties (`Q_PROPERTY`), set in C++ or in Qt Designer's Property Editor. The
widgets do not take their colours from the application palette; the one exception is `LedIndicator`, whose label uses
the palette's `WindowText` colour while its `textColor` is not set. The gallery sets the properties for a light and a
dark theme.

Class names below use `qiw`, the short alias of namespace `QtIndustrialWidgets`. Default values are given where this
page quotes them.

---

## Colour properties

### Radial gauge (`qiw::RadialGauge`)
- `dialColor`: dial face background.
- `bezelColor`: outer rim.
- `needleColor`: needle.
- `scaleColor`: graduation ticks.
- `textColor`: numeric labels.
- `readoutColor`: the digital readout in the normal zone and `---` (default `#f5f6fa`); in the warning and error zones
  the readout takes `warningColor` and `errorColor`.
- `normalColor`, `warningColor`, `errorColor`: the threshold arc bands (shown while `thresholdBandsVisible` is true).

### Linear gauge (`qiw::LinearGauge`)
- `troughColor`: background of the trough that holds the liquid column.
- `bezelColor`: outer rim.
- `liquidColor`: liquid column.
- `scaleColor`: graduation ticks.
- `textColor`: numeric labels.
- `readoutColor`: `---`, the readout of an invalid input (default `#f0f4f8`); a finite input is shown in the colour of
  its zone.
- `normalColor`, `warningColor`, `errorColor`: with `dynamicLiquidColor` set, the liquid column takes `errorColor`
  from the error threshold up, `warningColor` from the warning threshold up and `normalColor` below it, instead of
  `liquidColor`.

### Seven-segment display (`qiw::SevenSegmentDisplay`)
- `activeSegmentColor`: lit segments.
- `inactiveSegmentColor`: unlit segments (default: the default lit colour with alpha 28).
- `backgroundColor`: display panel background.
- `bezelColor`: outer frame.

### Switch (`qiw::IndustrialSwitch`)
- `plateColor`: mounting plate.
- `leverColor`: lever handle or rocker body.
- `ledColor`: pilot LED.
- `textColor`: labels.
- `guardColor`: safety guard cover (default `#dc3545`).

### Slider (`qiw::IndustrialSlider`)
- `trackColor`: the track (default `#1e242d`); the hatching outside the setpoint limits takes a colour derived from it.
- `normalColor`, `warningColor`, `errorColor`: the zone band along the track, below the first threshold and in the
  warning and error zones.
- `feedbackColor`: the feedback bar (default `#ff9f43`).
- `handleColor`: the setpoint handle (default `#00bcd4`).
- `scaleColor`: the ticks, the outline of the track and the stop marks of the setpoint limits.
- `textColor`: the scale labels and the focus ring.
- `readoutColor`: the setpoint and feedback readouts, drawn on a pod that is dark in every theme (default `#f0f4f8`).

### Thumbwheel switch (`qiw::ThumbwheelSwitch`)
- `drumColor`: the drum faces (default `#14181e`).
- `digitColor`: the digits, the sign and the arrows of the buttons, and the text of the SET button while a value can
  be applied.
- `buttonColor`: the faces of the buttons, and the outline of the drums when no value is pending (default `#343c48`).
- `textColor`: the decimal point, the unit, the marks of the tag column, the status line (padlock, ⊘, applied value),
  the outline of the SET button when there is nothing to apply, and the focus marks.
- `pendingColor`: the dashed outline of the drums while a value is pending (default `#ffb000`).

### Level meter (`qiw::LevelMeter`)
- `normalColor`: normal zone (default `#2ecc71`).
- `warningColor`: warning zone (default `#fed330`).
- `errorColor`: error zone (default `#eb3b5a`).
- `peakColor`: peak hold indicator.
- `backgroundColor`: channel trough background.
- `textColor`: scale ticks and labels.

### Annunciator (`qiw::AnnunciatorPanel`)
- `frameColor`: outer frame.
- `gridColor`: dividers between the tiles.
- `criticalColor`: lit `Critical` tiles (default `#eb3b5a`).
- `warningColor`: lit `Warning` tiles (default `#fed330`).
- `advisoryColor`: lit `Advisory` tiles (default `#00e5ff`).
- `textColor`: tile legends (default `#f0f4fa`).

### Compass (`qiw::Compass`)
- `dialColor`: compass card (`HeadingUp` mode) or fixed rose (`NorthUp` mode).
- `bezelColor`: outer rim.
- `textColor`: degree numbers, graduation ticks and the letters NE, SE, SW, NW.
- `readoutColor`: the digital heading readout, `---°` included (default `#e1e7ec`).
- `cardinalColor`: the letters E, S, W and the cardinal points of the rose.
- `needleColor`: north end of the needle and the letter N (default `#eb3b5a`).
- `needleTailColor`: south end of the needle (default `#a0afc3`).
- `bugColor`: heading bug (default `#fe8228`).
- `lubberColor`: lubber line at 12 o'clock (default `#fed330`).

### Other widgets
`LedIndicator` (`onColor`, `offColor`, `bezelColor`, `textColor`), `IndustrialKnob` (`knobColor`, `pointerColor`,
`scaleColor`, `trackColor`, `textColor`), `CircularBargraph` (`normalColor`, `warningColor`, `errorColor`,
`peakColor`, `ghostColor`, `dialColor`, `textColor`), `StripChart` (`gridColor`, `backgroundColor`, `bezelColor`) and
`AnalysisChart` (`gridColor`, `backgroundColor`, `bezelColor`, `textColor`, `cursorColor`) have colour properties as
well; their API pages describe them.

The readout pods of `RadialGauge`, `LinearGauge`, `Compass`, `IndustrialKnob` and `IndustrialSlider` are dark in every
theme, so a light theme sets `textColor` dark but keeps a light `readoutColor` (#159, see [widgets.md](widgets.md)).
`ThumbwheelSwitch` has no fixed pod: its drums follow `drumColor`, so a theme sets `drumColor` and `digitColor`
together.

---

## Example: a light and a dark theme

```cpp
void applyTheme(qiw::RadialGauge *gauge, bool dark)
{
    QColor bg = dark ? QColor(20, 24, 32) : QColor(245, 247, 250);
    QColor text = dark ? QColor(240, 242, 245) : QColor(30, 35, 45);
    QColor bezel = dark ? QColor(48, 56, 70) : QColor(190, 195, 205);

    gauge->setDialColor(bg);
    gauge->setTextColor(text);
    gauge->setBezelColor(bezel);
}
```
