<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->

# Qt Designer Integration Guide {#designer_plugin}

QtIndustrialWidgets provides an integrated plugin module (`QtIndustrialWidgetsPlugin`) that seamlessly registers all 11 widgets inside **Qt Designer** and **Qt Creator**.

---

## 🛠️ Building the Plugin

When CMake detects Qt Designer (`Qt6Designer` or `Qt5Designer`), the target `QtIndustrialWidgetsPlugin` is automatically configured:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target QtIndustrialWidgetsPlugin
```

This generates `libQtIndustrialWidgetsPlugin.so` (Linux), `.dylib` (macOS), or `.dll` (Windows).

---

## 📂 Installation Directories

Copy the built plugin binary into your Qt Designer or Qt Creator plugin directory:

### Linux
```bash
# Qt Creator plugins directory
cp build/plugin/libQtIndustrialWidgetsPlugin.so ~/.local/share/QtProject/QtCreator/plugins/

# Or system Qt Designer directory
cp build/plugin/libQtIndustrialWidgetsPlugin.so /usr/lib/x86_64-linux-gnu/qt6/plugins/designer/
```

### Windows
```text
C:\Qt\6.x.x\msvc2022_64\plugins\designer\QtIndustrialWidgetsPlugin.dll
```

### macOS
```text
~/Library/Application Support/QtProject/Qt Creator/plugins/libQtIndustrialWidgetsPlugin.dylib
```

---

## 🎨 Available Widgets in Designer

Once installed, a new category **"Industrial Widgets"** will appear in the Widget Box:

1. `QtIndustrialWidgets::RadialGauge`
2. `QtIndustrialWidgets::LinearGauge`
3. `QtIndustrialWidgets::SevenSegmentDisplay`
4. `QtIndustrialWidgets::LedIndicator`
5. `QtIndustrialWidgets::IndustrialKnob`
6. `QtIndustrialWidgets::StripChart`
7. `QtIndustrialWidgets::IndustrialSwitch`
8. `QtIndustrialWidgets::LevelMeter`
9. `QtIndustrialWidgets::AnnunciatorPanel`
10. `QtIndustrialWidgets::Compass`
11. `QtIndustrialWidgets::CircularBargraph`

All properties (ranges, thresholds, colors, orientations) can be customized directly within the **Property Editor**!
