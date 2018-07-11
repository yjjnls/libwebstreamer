#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import platform

from conans import CMake, ConanFile, RunEnvironment, tools


class TestPackageConan(ConanFile):
    def imports(self):
        if not os.path.exists("webstreamer"):
            self.run("git clone https://github.com/yjjnls/webstreamer.git")

            self.run("cd webstreamer && git checkout linux && sudo npm install")

    def requirements(self):
        self.requires("tesseract.plugin/0.2.2@%s/stable" %
                      os.environ.get("CONAN_USERNAME", "yjjnls"))

    def test(self):
        if platform.system() == "Windows":
            self.run("pip install cpplint")
        else:
            self.run("sudo pip install cpplint")

        # custom: source dir
        source_dir = "%s/../lib" % os.path.dirname(__file__)

        cpplint_options = "cpplint --filter=-whitespace/tab,-whitespace/braces,-build/header_guard,-readability/casting,-build/include_order,-build/include,-runtime/int --linelength=120 "
        for (root, dirs, files) in os.walk(source_dir):
            for filename in files:
                if 'nlohmann' in os.path.join(root, filename):
                    continue
                self.run("%s%s" % (cpplint_options,
                                   os.path.join(root, filename)))

        bin_path = ""
        if platform.system() == "Windows":
            for p in self.deps_cpp_info.bin_paths:
                bin_path = "%s%s%s" % (p, os.pathsep, bin_path)
            # vars = {'PATH': "%s%s" % (bin_path, os.environ["PATH"])}
        else:
            gstreamer_root = os.environ.get(
                "GSTREAMER_ROOT", "%s/gstreamer/linux_x86_64" % os.getenv("HOME"))
            for p in self.deps_cpp_info.lib_paths:
                bin_path = "%s%s%s" % (p, os.pathsep, bin_path)

        tesseract_plugin_path = self.deps_cpp_info['tesseract.plugin'].lib_paths[0]

        libwebstreamer_path = self.deps_cpp_info["libwebstreamer"].build_paths[0]

        if (not os.getenv("TESSDATA_PREFIX")):
            if platform.system() == "Windows":
                tessdata_dir = "c:\\"
            else:
                tessdata_dir = "/tmp"

        vars = {
            'LD_LIBRARY_PATH': '%s:%s' % (libwebstreamer_path, bin_path),
            'TESSERACT_PLUGIN_PATH': tesseract_plugin_path,
            'LIBWEBSTREAMER_PATH': libwebstreamer_path,
            'TESSDATA_PREFIX': tessdata_dir,
            'GST_DEBUG': 'webstreamer:5'
        }

        # custom: run test
        with tools.environment_append(vars):
            if platform.system() == "Windows":
                # command = "tesseract.plugin.test"
                pass
            else:
                load_gstreamer = "cd %s && chmod +x tshell.sh && ./tshell.sh && cd - " % gstreamer_root
                command1 = "&& npm test test/webstreamer.test.js "
                command2 = "&& npm test test/livestream.test.js "
            self.run("%s%s%s" % (load_gstreamer, command1, command2),
                     cwd="%s/webstreamer" % os.getcwd())
