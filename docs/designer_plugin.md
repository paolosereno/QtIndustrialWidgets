<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>

SPDX-License-Identifier: MIT
-->

# Qt Designer plugin {#designer_plugin}

The plugin module `QtIndustrialWidgetsPlugin` registers the 16 widgets in **Qt Widgets Designer** and in the Designer
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

`cmake --install build` copies the plugin to a directory chosen at configure time (#135), in this order:

1. **`QT_INSTALL_PLUGINS` set by hand**: `${QT_INSTALL_PLUGINS}/designer`. It always takes precedence.
2. **Installing into Qt's own prefix** (`CMAKE_INSTALL_PREFIX` is the same directory as the prefix of the Qt the
   library is built with, `QT6_INSTALL_PREFIX`): Qt's plugin directory, `<prefix>/${QT6_INSTALL_PLUGINS}/designer`.
   This is where that Qt's Designer looks, so it loads the plugin with no further setting. Examples:
   `<Qt>/plugins/designer/` for Qt from the online installer or `aqt` (checked in CI with `aqt`'s Qt 6.2.4), and
   `/usr/lib/x86_64-linux-gnu/qt6/plugins/designer/` for Ubuntu's Qt with the prefix `/usr`.
3. **Otherwise**, for example `/usr/local` or `/opt/QtIndustrialWidgets`: `<prefix>/lib/qt6/plugins/designer/`
   (`lib` is `CMAKE_INSTALL_LIBDIR`), a directory Designer does not scan; start Designer with
   `QT_PLUGIN_PATH=<prefix>/lib/qt6/plugins`, or set `QT_INSTALL_PLUGINS`.

The configure step prints the destination and the reason, for example
`QtIndustrialWidgets: Designer plugin destination: plugins/designer (Qt's plugin directory: CMAKE_INSTALL_PREFIX is
Qt's prefix)`. The two prefixes are compared after resolving symbolic links, `.` and `..` (and without letter case on
Windows). The decision uses the `CMAKE_INSTALL_PREFIX` of the configure step: `cmake --install build --prefix <other>`
keeps the destination relative to the prefix, as chosen at configure time. Not checked in CI: Homebrew's Qt on macOS
and the online installer on macOS and Windows.

The plugin links the library, `libQtIndustrialWidgets`. On Linux and macOS the installed plugin finds it through its
run path: first the library directory relative to the plugin (`$ORIGIN/…`, `@loader_path/…`), so the installed
prefix can be moved, then the absolute library directory (`CMAKE_INSTALL_FULL_LIBDIR`), for a plugin directory
outside the prefix. No `LD_LIBRARY_PATH` is needed.

On Windows there is no run path: `QtIndustrialWidgets.dll` must be in a directory listed in `PATH` or next to
`designer.exe`, otherwise Designer does not load the plugin.

### Prebuilt Linux packages

The `.deb` and the `.tar.gz` of the [GitHub Releases](https://github.com/paolosereno/QtIndustrialWidgets/releases)
are built on Ubuntu 24.04 with Ubuntu's Qt, with the prefix `/usr` and the multiarch library directory
`lib/x86_64-linux-gnu` (both packages are built and checked on Ubuntu 24.04):

- **`.deb`**: the plugin is installed to `/usr/lib/x86_64-linux-gnu/qt6/plugins/designer/`, Qt's own plugin directory
  on Ubuntu (`qtpaths6 --plugin-dir`), so Qt Designer loads it with no further setting. The library is in
  `/usr/lib/x86_64-linux-gnu/`. The `linux-packages.yml` workflow installs the `.deb` with apt and checks that Qt Designer
  loads the plugin.
- **`.tar.gz`**: the plugin is in `<extracted dir>/lib/x86_64-linux-gnu/qt6/plugins/designer/`, a directory Qt does not
  scan. Qt Designer finds it only with `QT_PLUGIN_PATH` set to the parent of `designer/`:

  ```bash
  QT_PLUGIN_PATH=<extracted dir>/lib/x86_64-linux-gnu/qt6/plugins designer
  ```

The `.deb` of v2.5.0 installed the plugin to `/usr/lib/qt6/plugins/designer/`, where Qt Designer does not
look: with that package, start Designer with `QT_PLUGIN_PATH=/usr/lib/qt6/plugins`.

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
13. `QtIndustrialWidgets::TankDisplay`
14. `QtIndustrialWidgets::ValveIndicator`
15. `QtIndustrialWidgets::IndustrialSlider`
16. `QtIndustrialWidgets::ThumbwheelSwitch`

The Property Editor shows each widget's `Q_PROPERTY`s (ranges, thresholds, colours, orientation, …).
