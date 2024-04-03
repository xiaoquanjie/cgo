# This file is managed by Conan, contents will be overwritten.
# To keep your changes, remove these comment lines, but the plugin won't be able to modify your requirements

from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake, CMakeToolchain
from conan.tools.files import get
from conan.tools.scm import Git

class ConanApplication(ConanFile):
    name = "grpc"
    version = "1.62.1"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def source(self):
        self.run("git clone -b master https://github.com/grpc/grpc")
        # download 比较新版本的grpcm, 因为conancenter上的版本相对老旧
        #get(self, "https://github.com/grpc/grpc/archive/refs/tags/v1.62.1.zip", strip_root=True)
        #git = Git(self)
        #git.clone(url="git@github.com:grpc/grpc.git", target=".")

    def package_info(self):
        self.cpp_info.libs = ["grpc"]

    def build_requirements(self):
        pass
        #self.tool_requires("autoconf/2.72")
        #self.tool_requires("libtool/2.4.7")