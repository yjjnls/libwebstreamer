#!/usr/bin/env python
# -*- coding: utf-8 -*-

from conans import ConanFile, CMake, tools
import os
import platform

__dir__ = os.path.dirname(os.path.abspath(__file__))


class LibWebstreamerConan(ConanFile):
    name = "libwebstreamer"
    version = "0.1.0-dev"
    description = "custom modified gstreamer library"
    url = "https://github.com/yjjnls/conan-gstreamer"
    license = "Apache-2.0"
    homepage = "https://github.com/yjjnls/conan-gstreamer"

    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = "shared=True", "fPIC=True"
    source_subfolder = "source_subfolder"
    generators = "cmake"

    def config_options(self):
        # self.root = "%s" % os.getcwd()
        if self.settings.os == "Windows":
            self.options.remove("fPIC")

    def source(self):
        self.run("git config --global user.name \"yjjnls\"")
        self.run("git config --global user.email \"x-jj@foxmail.com\"")

    def requirements(self):
        try:
            if self.settings.os == 'Linux':
                self.run("sudo conan remote add upload_${CONAN_USERNAME} \
                https://api.bintray.com/conan/${CONAN_USERNAME}/stable --insert 0"
                         )
        except Exception as e:
            print "The repo may have been added, the error above can be ignored."

        self.requires("gstreamer-runtime/1.14.0.1@%s/stable" % os.environ.get(
            "CONAN_USERNAME", "yjjnls"))
        # self.requires("tesseract.plugin/0.2.0@%s/stable" % os.environ.get(
        #     "CONAN_USERNAME", "yjjnls"))

        pass

    def build(self):
        self.requires("gstreamer-dev/1.14.0.1@%s/stable" % os.environ.get(
            "CONAN_USERNAME", "yjjnls"))
        self.requires("gstreamer-custom/1.14.0.1@%s/stable" % os.environ.get(
            "CONAN_USERNAME", "yjjnls"))

        self.run("git clone https://github.com/yjjnls/libwebstreamer.git")
        self.run("cd libwebstreamer && git checkout linux")

        if self.settings.os == "Linux":
            gstreamer_root = os.environ.get("GSTREAMER_ROOT",
                                            "/opt/gstreamer/linux_x86_64")

            vars = {
                'PKG_CONFIG_PATH': "%s/lib/pkgconfig" % gstreamer_root,
                'GSTREAMER_ROOT': gstreamer_root
            }

        cmake = CMake(self)
        with tools.environment_append(vars):
            cmake.configure(
                source_folder='libwebstreamer',
                build_folder='build/libwebstreamer')
            cmake.build(build_dir='build/libwebstreamer')

            ld_path = ""
            for p in self.deps_cpp_info.lib_paths:
                ld_path = "%s%s%s" % (p, os.pathsep, ld_path)
            print "ld_path:\n%s" % ld_path

    def package(self):
        ext = '.dll'
        if self.settings.os == 'Linux':
            ext = '.so'
        self.copy(
            pattern="libwebstreamer%s" % ext,
            dst=".",
            src="build/libwebstreamer/lib")

    # def package_info(self):
    #     if self.settings.os == "Linux":
    #         gstreamer_root = os.environ.get("GSTREAMER_ROOT",
    #                                         "/opt/gstreamer/linux_x86_64")

    #         self.run("sudo rm -rf out.bak && sudo cp -rf out out.bak")
    #         self.run("sudo mv out.bak/libgstrtspserver-1.0.so %s/lib" %
    #                  gstreamer_root)
    #         self.run("sudo mv out.bak/libgstdtls.so %s/lib/gstreamer-1.0" %
    #                  gstreamer_root)
    #         self.run("sudo rm -rf out.bak")
