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
- **Qt 6.2+** with `Core`, `Gui`, and `Widgets` modules. Qt 5.15 is not supported at the moment, see [#53](https://github.com/paolosereno/QtIndustrialWidgets/issues/53).

---

## 🛠️ Integration Methods

### Method 1: FetchContent (Recommended)

The easiest way to use QtIndustrialWidgets is directly via CMake's `FetchContent`. Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
    QtIndustrialWidgets
    GIT_REPOSITORY https://github.com/paolosereno/QtIndustrialWidgets.git
    GIT_TAG        v2.2.0
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

Using vcpkg with manifest mode or ports:

```bash
vcpkg install --overlay-ports=./ports/qtindustrialwidgets qtindustrialwidgets
```

### Method 5: Conan 2.0 Package Manager

Using Conan:

```bash
conan install . --output-folder=build --build=missing
```

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
