#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os

from conans import CMake, ConanFile, tools

__dir__ = os.path.dirname(os.path.abspath(__file__))


class LibWebstreamerConan(ConanFile):
    name = "libwebstreamer"
    version = "0.1.0"
    description = "custom modified gstreamer library"
    url = "https://github.com/yjjnls/conan-gstreamer"
    license = "Apache-2.0"
    homepage = "https://github.com/yjjnls/conan-gstreamer"

    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = "shared=True", "fPIC=True"
    source_subfolder = "source_subfolder"

    exports_sources = 'CMakeLists.txt', 'conan.cmake', 'lib/*'
    generators = "cmake"
    gstreamer_root = ""

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.remove("fPIC")

    def source(self):
        self.run("git config --global user.name \"yjjnls\"")
        self.run("git config --global user.email \"x-jj@foxmail.com\"")

    def requirements(self):
        try:
            if self.settings.os == 'Linux':
                self.run("sudo conan remote add upload_%s \
                https://api.bintray.com/conan/%s/stable --insert 0 >/dev/null"
                         % (os.environ.get("DEPENDENT_BINTRAY_REPO"),
                            os.environ.get("DEPENDENT_BINTRAY_REPO")))
        except Exception as e:
            print "The repo may have been added, the error above can be ignored."

        self.requires("gstreamer-runtime/1.14.0.1@%s/stable" %
                      os.environ.get("DEPENDENT_BINTRAY_REPO"))
        self.requires("gstreamer-custom/1.14.0.1@%s/stable" %
                      os.environ.get("DEPENDENT_BINTRAY_REPO"))

    def build_requirements(self):
        if self.settings.os == "Linux":
            self.build_requires("gstreamer-dev/1.14.0.1@%s/stable" %
                                os.environ.get("DEPENDENT_BINTRAY_REPO"))

    def build(self):
        if self.settings.os == 'Linux':
            self.gstreamer_root = os.environ.get(
                "GSTREAMER_ROOT", "%s/gstreamer/linux_x86_64" % os.getenv("HOME"))
            os.environ["GSTREAMER_ROOT"] = self.gstreamer_root
            self.run("sudo mkdir -p %s", self.gstreamer_root)
        else:
            pass
        if self.settings.os == 'Linux':
            self.run(
                "if [ `expr $(pkg-config --version) \< 0.29.1` -ne 0 ]; then \
                cd /tmp \
                && wget https://pkg-config.freedesktop.org/releases/pkg-config-0.29.1.tar.gz \
                && tar -zxf pkg-config-0.29.1.tar.gz \
                && cd pkg-config-0.29.1 \
                && ./configure --prefix=/usr        \
                --with-internal-glib \
                --disable-host-tool  \
                --docdir=/usr/share/doc/pkg-config-0.29.1 \
                && make \
                && sudo make install; fi")

        if self.settings.os == "Linux":
            vars = {'PKG_CONFIG_PATH': "%s/lib/pkgconfig" % self.gstreamer_root,
                    'GSTREAMER_ROOT': self.gstreamer_root}

        cmake = CMake(self)
        with tools.environment_append(vars):
            cmake.configure(build_folder='build/libwebstreamer')
            cmake.build(build_dir='build/libwebstreamer')

            # ld_path = ""
            # for p in self.deps_cpp_info.lib_paths:
            #     ld_path = "%s%s%s" % (p, os.pathsep, ld_path)
            # print "ld_path:\n%s" % ld_path

    def package(self):
        ext = '.dll'
        if self.settings.os == 'Linux':
            ext = '.so'
        self.copy(
            pattern="libwebstreamer%s" % ext,
            dst=".",
            src="build/libwebstreamer/lib")
