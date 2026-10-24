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
- `normalColor`, `warningColor`, `errorColor`: the threshold arc bands (shown while `thresholdBandsVisible` is true).

### Linear gauge (`qiw::LinearGauge`)
- `troughColor`: background of the trough that holds the liquid column.
- `bezelColor`: outer rim.
- `liquidColor`: liquid column.
- `scaleColor`: graduation ticks.
- `textColor`: numeric labels.
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
