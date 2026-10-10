# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os

class QtIndustrialWidgetsConan(ConanFile):
    name = "qtindustrialwidgets"
    version = "2.1.0"
    license = "MIT"
    author = "Paolo Sereno <paolomsereno@gmail.com>"
    url = "https://github.com/paolosereno/QtIndustrialWidgets"
    description = "Modern, high-performance industrial instruments and SCADA widgets for Qt"
    topics = ("qt", "widgets", "scada", "instrumentation", "telemetry")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": True, "fPIC": True}

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_TESTS"] = False
        tc.variables["BUILD_DOCS"] = False
        tc.variables["BUILD_DESIGNER_PLUGIN"] = False
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
        self.cpp_info.libs = ["QtIndustrialWidgets"]
