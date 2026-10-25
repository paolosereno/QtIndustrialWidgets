# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Conan 2 recipe of QtIndustrialWidgets. Qt comes from ConanCenter (qt/6.x); test_package/ builds a consumer that
# finds the package, links a widget and checks that it draws (#66). Checked in CI by .github/workflows/packaging.yml.

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class QtIndustrialWidgetsConan(ConanFile):
    name = "qtindustrialwidgets"
    version = "2.5.1"
    license = "MIT"
    author = "Paolo Sereno <pablitos1024@gmail.com>"
    url = "https://github.com/paolosereno/QtIndustrialWidgets"
    homepage = "https://github.com/paolosereno/QtIndustrialWidgets"
    description = "C++17 instrumentation widgets for Qt Widgets"
    topics = ("qt", "widgets", "scada", "instrumentation", "telemetry")
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    # Static by default, like ConanCenter's qt: CMakeToolchain passes the choice as BUILD_SHARED_LIBS (#107)
    default_options = {"shared": False, "fPIC": True}
    # The library sources; the tests, examples, benchmarks, plugin and docs are not built by this recipe
    exports_sources = "CMakeLists.txt", "cmake/*", "include/*", "src/*", "LICENSES/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder=".")

    def validate(self):
        # A shared library built against a static Qt carries its own copy of Qt: a widget created by the library would
        # not see the application's QApplication ("QWidget: Must construct a QApplication before a QWidget")
        if self.options.shared and not self.dependencies["qt"].options.shared:
            raise ConanInvalidConfiguration(
                f"{self.ref} shared=True needs a shared Qt (qt/*:shared=True); with a static Qt use shared=False")

    def requirements(self):
        # Qt 6.2 is the minimum (CMakeLists.txt: find_package(Qt6 6.2 ...)); the public headers include Qt headers
        self.requires("qt/[>=6.2 <7]", transitive_headers=True, transitive_libs=True)

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_TESTS"] = False
        tc.variables["BUILD_DOCS"] = False
        tc.variables["BUILD_DESIGNER_PLUGIN"] = False
        tc.variables["BUILD_BENCHMARKS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "MIT.txt", src=os.path.join(self.source_folder, "LICENSES"), dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        # Same package and target names as the CMake config the library installs
        self.cpp_info.set_property("cmake_file_name", "QtIndustrialWidgets")
        self.cpp_info.set_property("cmake_target_name", "QtIndustrialWidgets::QtIndustrialWidgets")
        self.cpp_info.libs = ["QtIndustrialWidgets"]
        if not self.options.shared:
            # Empties QTINDUSTRIALWIDGETS_EXPORT for consumers, as the CMake target does (qtindustrialwidgets_global.h)
            self.cpp_info.defines = ["QTINDUSTRIALWIDGETS_STATIC"]
        self.cpp_info.requires = ["qt::qtCore", "qt::qtGui", "qt::qtWidgets"]
