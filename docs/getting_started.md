# Getting Started with QtIndustrialWidgets {#getting_started}

Welcome to the **QtIndustrialWidgets** documentation! This guide will help you integrate the library into your C++ and Qt applications.

---

## 📦 Requirements

- **C++17** compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- **CMake 3.16** or newer
- **Qt 6** (6.2+) or **Qt 5** (5.15 LTS) with `Core`, `Gui`, and `Widgets` modules.

---

## 🛠️ Integration Methods

### Method 1: FetchContent (Recommended)

The easiest way to use QtIndustrialWidgets is directly via CMake's `FetchContent`. Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
    QtIndustrialWidgets
    GIT_REPOSITORY https://github.com/paolosereno/QtIndustrialWidgets.git
    GIT_TAG        main
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

---

## 💡 Quick Code Example

Here is a minimal example using `QRadialGauge` and `QLedIndicator`:

```cpp
#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>
#include <QtIndustrialWidgets/QRadialGauge.h>
#include <QtIndustrialWidgets/QLedIndicator.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    QVBoxLayout layout(&window);

    // Create a circular tachometer
    auto *gauge = new QRadialGauge(&window);
    gauge->setRange(0.0, 8000.0);
    gauge->setValue(3500.0);
    gauge->setUnit(QStringLiteral("RPM"));
    gauge->setWarningThreshold(6000.0);
    gauge->setErrorThreshold(7200.0);

    // Create a status LED
    auto *led = new QLedIndicator(QColor(46, 204, 113), &window);
    led->setLabelText(QStringLiteral("SYSTEM RUNNING"));
    led->setOn(true);

    layout.addWidget(gauge);
    layout.addWidget(led);

    window.resize(300, 400);
    window.show();

    return app.exec();
}
```
