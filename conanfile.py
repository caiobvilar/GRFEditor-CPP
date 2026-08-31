import os
import shutil

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class GrfEditorConan(ConanFile):
    name = "grfeditor"
    version = "0.2.0"
    license = "CC-BY-NC-SA-4.0"
    description = "Cross-platform editor for Ragnarok Online GRF/GPF/Thor archives and associated file formats."
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires("sdl/3.4.14")
        self.requires("imgui/1.92.8-docking")
        self.requires("glad/2.0.8")
        self.requires("zlib/1.3.1")
        self.requires("gtest/1.16.0")

    def configure(self):
        self.options["glad"].gl_profile = "core"
        self.options["glad"].gl_version = "3.3"

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()

        # The conan-center imgui package builds only the core library. The platform
        # backend sources live in <pkg>/res/bindings; consumers are expected to
        # compile the backends they need. Copy them into our build tree so the
        # CMake target below can pick them up without fighting with package paths.
        imgui = self.dependencies["imgui"]
        bindings_src = os.path.join(str(imgui.package_folder), "res", "bindings")
        bindings_dst = os.path.join(self.build_folder, "imgui_backends")
        os.makedirs(bindings_dst, exist_ok=True)
        for filename in (
            "imgui_impl_sdl3.h",
            "imgui_impl_sdl3.cpp",
            "imgui_impl_opengl3.h",
            "imgui_impl_opengl3.cpp",
            "imgui_impl_opengl3_loader.h",
        ):
            shutil.copy2(
                os.path.join(bindings_src, filename),
                os.path.join(bindings_dst, filename),
            )