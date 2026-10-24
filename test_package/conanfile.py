# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Conan test_package (#66): builds the consumer next to this file against the created package and runs it.

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
import os


class QtIndustrialWidgetsTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualRunEnv"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        # A static Qt (ConanCenter's default) does not load plugins at run time, and its recipe has no CMake target for
        # the offscreen platform plugin: give the consumer the plugin library and the object that registers it
        qt = self.dependencies["qt"]
        if not qt.options.shared:
            msvc = self.settings.compiler == "msvc"
            platforms = os.path.join(qt.package_folder, "plugins", "platforms")
            init = os.path.join(platforms, f"objects-{self.settings.build_type}", "QOffscreenIntegrationPlugin_init",
                                "QOffscreenIntegrationPlugin_init.cpp" + (".obj" if msvc else ".o"))
            library = os.path.join(platforms, "qoffscreen.lib" if msvc else "libqoffscreen.a")
            for path in (init, library):
                if not os.path.isfile(path):
                    raise ConanException(f"the static offscreen plugin is not where expected: {path}")
            # Forward slashes: the paths end up in a CMake file, where backslashes would be escapes
            tc.variables["QIW_CONSUMER_STATIC_PLUGIN"] = ";".join(p.replace("\\", "/") for p in (init, library))
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            self.run(os.path.join(self.cpp.build.bindir, "consumer"), env="conanrun")
