# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

# Conan test_package (#66): builds the consumer next to this file against the created package and runs it.

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout
import os


class QtIndustrialWidgetsTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain", "VirtualRunEnv"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            self.run(os.path.join(self.cpp.build.bindir, "consumer"), env="conanrun")
