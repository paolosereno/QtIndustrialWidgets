<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->

# Theming and Styling Guide {#theming}

QtIndustrialWidgets is designed with modern SCADA and dashboard theming in mind. All widgets render crisply across both **Light** and **Dark** themes.

---

## 🎨 Built-in Color Properties

Each widget exposes fine-grained styling properties that can be set in C++, via QML/Qt Designer, or adjusted dynamically:

### Gauges (`QRadialGauge`, `QLinearGauge`)
- `dialColor`: Background color of the gauge face.
- `bezelColor`: Outer rim / bezel gradient.
- `needleColor`: Needle / pointer color.
- `textColor`: Tick marks and numeric text.
- `warningColor` & `errorColor`: Color of threshold zones and liquid columns.

### Displays (`QSevenSegmentDisplay`)
- `activeSegmentColor`: Lit segment illumination color.
- `inactiveSegmentColor`: Unlit segment ghost transparency color.
- `backgroundColor`: LCD / LED background matrix color.
- `bezelColor`: Bezel border color.

### Switches (`QIndustrialSwitch`)
- `plateColor`: Base panel plate metal color.
- `leverColor`: Bat lever or rocker body color.
- `ledColor`: Integrated status indicator color.
- `guardColor`: Safety flip-cover color (crimson red or hazard yellow).

### Level Meters (`QLevelMeter`)
- `normalColor`: Normal level color (default Green: `#2ecc71`).
- `warningColor`: Caution level color (default Amber: `#fed330`).
- `errorColor`: Overload / peak level color (default Red: `#eb3b5a`).
- `backgroundColor`: Recessed track / chassis color.

### Annunciators (`QAnnunciatorPanel`)
- `frameColor`: Heavy chassis outer bezel frame color.
- `gridColor`: Internal window matrix divider bar color.
- `criticalColor`: High-priority / emergency alarm illumination color (Crimson Red: `#eb3b5a`).
- `warningColor`: Medium-priority warning illumination color (Amber Gold: `#fed330`).
- `advisoryColor`: Low-priority advisory illumination color (Cyan: `#00e5ff`).
- `textColor`: Tile engraved legend lettering color (`#f0f4fa`).

### Compasses (`QCompass`)
- `dialColor`: Background color of the rotating card or fixed compass face.
- `bezelColor`: Machined metallic outer rim and inner groove.
- `textColor`: Degree numbers and graduation ticks.
- `cardinalColor`: Cardinal directions highlight color (N/E/S/W).
- `needleColor`: North pointer arrow color (Vivid Red: `#eb3b5a`).
- `needleTailColor`: South pointer arrow color (Slate: `#a0afc3`).
- `bugColor`: Target heading bug chevron color (Fluorescent Orange: `#fe8228`).
- `lubberColor`: 12 o'clock reference lubber line color (Safety Amber: `#fed330`).

---

## 🌙 Dark vs Light Theme Example

```cpp
void applyTheme(bool dark)
{
    QColor bg = dark ? QColor(20, 24, 32) : QColor(245, 247, 250);
    QColor text = dark ? QColor(240, 242, 245) : QColor(30, 35, 45);
    QColor bezel = dark ? QColor(48, 56, 70) : QColor(190, 195, 205);

    gauge->setDialColor(bg);
    gauge->setTextColor(text);
    gauge->setBezelColor(bezel);
}
```
