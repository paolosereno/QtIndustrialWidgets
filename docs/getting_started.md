<!--
SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>

SPDX-License-Identifier: MIT
-->

# Getting Started with QtIndustrialWidgets {#getting_started}

Welcome to the **QtIndustrialWidgets** documentation! This guide will help you integrate the library into your C++ and Qt applications.

---

## 📦 Requirements

- **C++17** compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- **CMake 3.16** or newer
- **Qt 6.2+** with `Core`, `Gui`, and `Widgets` modules. Qt 5 is not supported; the minimum is Qt 6.2 (see [#53](https://github.com/paolosereno/QtIndustrialWidgets/issues/53)).

---

## 🛠️ Integration Methods

### Method 1: FetchContent (Recommended)

The easiest way to use QtIndustrialWidgets is directly via CMake's `FetchContent`. Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
    QtIndustrialWidgets
    GIT_REPOSITORY https://github.com/paolosereno/QtIndustrialWidgets.git
    GIT_TAG        v2.5.0
)

# Optional: Disable examples and designer plugin when fetching
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_DESIGNER_PLUGIN OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(QtIndustrialWidgets)

# Link to your application target
target_link_libraries(my_app
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
)
```

Pinning `GIT_TAG` to a release tag is recommended over `main`, which can change at any time and break reproducible builds.

### Shared or static library

The library is **shared** by default. It follows `BUILD_SHARED_LIBS` when that is set, on the command line
(`-DBUILD_SHARED_LIBS=OFF` for a static library) or by the project that adds it with `FetchContent`. A static library
is needed with a static Qt: a shared library built against a static Qt would carry its own copy of Qt. The
`QtIndustrialWidgets::QtIndustrialWidgets` target gives its consumers the `QTINDUSTRIALWIDGETS_STATIC` definition in a
static build, so nothing else is needed in the consuming project.

### Method 2: find_package (System or Local Install)

If you compile and install the library:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /opt/QtIndustrialWidgets
```

In your application's `CMakeLists.txt`:

```cmake
find_package(QtIndustrialWidgets REQUIRED)

target_link_libraries(my_app
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
)
```

### Method 3: Debian / Ubuntu Native Package (.deb)

Download the official `.deb` package from the [GitHub Releases](https://github.com/paolosereno/QtIndustrialWidgets/releases) page and install:

```bash
sudo dpkg -i qtindustrialwidgets_<version>_amd64.deb
```

Replace `<version>` with the version of the package you downloaded from [Releases](https://github.com/paolosereno/QtIndustrialWidgets/releases).

Then simply use `find_package(QtIndustrialWidgets REQUIRED)` in your `CMakeLists.txt`.

### Method 4: vcpkg C++ Package Manager

The port is in [`ports/qtindustrialwidgets`](https://github.com/paolosereno/QtIndustrialWidgets/tree/main/ports/qtindustrialwidgets) and is used as an overlay port. It is
**experimental**: see the README. With a triplet (here `x64-linux-release`, a static Linux build):

```bash
# Manifest mode: "qtindustrialwidgets" in your project's vcpkg.json dependencies
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
      -DVCPKG_TARGET_TRIPLET=x64-linux-release \
      -DVCPKG_OVERLAY_PORTS=/path/to/QtIndustrialWidgets/ports

# Classic mode
vcpkg install qtindustrialwidgets:x64-linux-release --overlay-ports=/path/to/QtIndustrialWidgets/ports
```

Then `find_package(QtIndustrialWidgets CONFIG REQUIRED)` and link `QtIndustrialWidgets::QtIndustrialWidgets`.

The library follows the triplet's linkage. With a static triplet both Qt and QtIndustrialWidgets are static libraries
(see *Shared or static library* above and the note on a static Qt below).

### Method 5: Conan 2.0 Package Manager

The recipe is [`conanfile.py`](https://github.com/paolosereno/QtIndustrialWidgets/blob/main/conanfile.py), with Qt from ConanCenter. It builds a **static** library by default
(`shared=False`), as ConanCenter's `qt` is static by default:

```bash
# From a clone of this repository: build the package into the local Conan cache and run its test_package
conan create . --build=missing -s build_type=Release -o "qtindustrialwidgets/*:shared=False"
```

Then require `qtindustrialwidgets/2.5.0` in your project's conanfile, use `find_package(QtIndustrialWidgets CONFIG
REQUIRED)` and link `QtIndustrialWidgets::QtIndustrialWidgets`. `shared=True` needs a shared Qt
(`-o "qt/*:shared=True"`); with a static Qt the recipe refuses it, because a shared library built against a static Qt
carries its own copy of Qt. On Linux, Qt's `xorg/system` dependency installs the X11 development packages with the
system package manager (`-c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True`).

### Note: a static Qt

A static Qt does not load plugins at run time: the platform plugins the application uses are linked into the
executable. With Qt's own CMake packages (vcpkg) this is done with `qt_import_plugins()` (Qt's documentation lists
the plugins linked by default), for example `qt_import_plugins(app INCLUDE Qt6::QOffscreenIntegrationPlugin)`. ConanCenter's
`qt` recipe has no CMake target for the plugins; [`test_package/conanfile.py`](https://github.com/paolosereno/QtIndustrialWidgets/blob/main/test_package/conanfile.py) shows how
the consumer in this repository links the offscreen plugin from the `qt` package. Both setups are checked on Linux by
the packaging workflow ([`packaging.yml`](https://github.com/paolosereno/QtIndustrialWidgets/blob/main/.github/workflows/packaging.yml)).

---

## 💡 Quick Code Example

Here is a minimal example using `qiw::RadialGauge` and `qiw::LedIndicator`:

```cpp
#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>
#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/LedIndicator.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    QVBoxLayout layout(&window);

    // Using the compact namespace alias 'qiw' (or namespace QtIndustrialWidgets)
    auto *gauge = new qiw::RadialGauge(&window);
    gauge->setRange(0.0, 8000.0);
    gauge->setValue(3500.0);
    gauge->setUnit(QStringLiteral("RPM"));
    gauge->setWarningThreshold(6000.0);
    gauge->setErrorThreshold(7200.0);

    // Create a status LED
    auto *led = new qiw::LedIndicator(QColor(46, 204, 113), &window);
    led->setLabelText(QStringLiteral("SYSTEM RUNNING"));
    led->setOn(true);

    layout.addWidget(gauge);
    layout.addWidget(led);

    window.resize(300, 400);
    window.show();

    return app.exec();
}
```
