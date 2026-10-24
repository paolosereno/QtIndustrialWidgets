<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->

# Qt Designer plugin {#designer_plugin}

The plugin module `QtIndustrialWidgetsPlugin` registers the 12 widgets in **Qt Widgets Designer** and in the Designer
integrated in **Qt Creator**.

## Build

The option `BUILD_DESIGNER_PLUGIN` is on by default. CMake builds the plugin when it finds Qt `Designer`; otherwise it
prints `Qt Designer NOT found, disabling designer plugin` and switches the option off.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target QtIndustrialWidgetsPlugin
```

The plugin is a CMake module library written to `build/lib/`: `QtIndustrialWidgetsPlugin.so` on Linux and macOS
(CMake's module suffix on macOS is `.so`), `QtIndustrialWidgetsPlugin.dll` on Windows.

## Install

`cmake --install build` copies the plugin to `<prefix>/lib/qt6/plugins/designer/` (`lib` is `CMAKE_INSTALL_LIBDIR`;
setting `QT_INSTALL_PLUGINS` at configure time changes the destination to `${QT_INSTALL_PLUGINS}/designer`).

To use the plugin from the build tree, copy it into the `designer/` folder of a Qt plugin directory, for example the
one of the Qt that built it (`qtpaths6 --plugin-dir`, on Ubuntu `/usr/lib/x86_64-linux-gnu/qt6/plugins`):

```bash
cp build/lib/QtIndustrialWidgetsPlugin.so "$(qtpaths6 --plugin-dir)/designer/"
```

The Designer integrated in Qt Creator loads plugins from `<QtCreator>/lib/Qt/plugins/designer` on Linux,
`<QtCreator>\bin\plugins\designer` on Windows and `Qt Creator.app/Contents/PlugIns/designer` on macOS, and only
plugins built with the same Qt version and compiler as Qt Creator itself; otherwise use the standalone Designer (see
[Adding Qt Designer Plugins](https://doc.qt.io/qtcreator/adding-plugins.html) in the Qt Creator manual).

## Widgets in Designer

The plugin adds the category **Industrial Widgets** to the Widget Box, with these classes:

1. `QtIndustrialWidgets::RadialGauge`
2. `QtIndustrialWidgets::LinearGauge`
3. `QtIndustrialWidgets::SevenSegmentDisplay`
4. `QtIndustrialWidgets::LedIndicator`
5. `QtIndustrialWidgets::IndustrialKnob`
6. `QtIndustrialWidgets::StripChart`
7. `QtIndustrialWidgets::AnalysisChart`
8. `QtIndustrialWidgets::IndustrialSwitch`
9. `QtIndustrialWidgets::LevelMeter`
10. `QtIndustrialWidgets::AnnunciatorPanel`
11. `QtIndustrialWidgets::Compass`
12. `QtIndustrialWidgets::CircularBargraph`

The Property Editor shows each widget's `Q_PROPERTY`s (ranges, thresholds, colours, orientation, …).
