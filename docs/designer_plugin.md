# Qt Designer Integration Guide {#designer_plugin}

QtIndustrialWidgets provides an integrated plugin module (`QtIndustrialWidgetsPlugin`) that seamlessly registers all 9 widgets inside **Qt Designer** and **Qt Creator**.

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

1. `QRadialGauge`
2. `QLinearGauge`
3. `QSevenSegmentDisplay`
4. `QLedIndicator`
5. `QIndustrialKnob`
6. `QStripChart`
7. `QIndustrialSwitch`
8. `QLevelMeter`
9. `QAnnunciatorPanel`

All properties (ranges, thresholds, colors, orientations) can be customized directly within the **Property Editor**!
