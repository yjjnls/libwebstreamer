#!/usr/bin/env python
# -*- coding: utf-8 -*-

from conans import ConanFile, CMake, tools, RunEnvironment
import os
import platform


class TestPackageConan(ConanFile):
    def system_requirements(self):
        print "-------------------------------------"
        self.requires("tesseract.plugin/0.2.0@%s/stable" % os.environ.get(
            "CONAN_USERNAME", "yjjnls"))

    def test(self):
        if platform.system() == "Windows":
            self.run("pip install cpplint")
        else:
            self.run("sudo pip install cpplint")

        # custom: source dir
        source_dir = "%s/../lib" % os.path.dirname(__file__)

        for (root, dirs, files) in os.walk(source_dir):
            for filename in files:
                if 'nlohmann' in os.path.join(root, filename):
                    continue
                self.run(
                    "cpplint --filter=-whitespace/tab,-whitespace/braces,-build/header_guard,-readability/casting,-build/include_order,-build/include,-runtime/int --linelength=120 %s"
                    % os.path.join(root, filename))

        # bin_path = ""
        # if platform.system() == "Windows":
        #     for p in self.deps_cpp_info.bin_paths:
        #         bin_path = "%s%s%s" % (p, os.pathsep, bin_path)
        #     vars = {'PATH': "%s%s" % (bin_path, os.environ["PATH"])}
        # else:
        #     for p in self.deps_cpp_info.lib_paths:
        #         bin_path = "%s%s%s" % (p, os.pathsep, bin_path)
        #     vars = {'LD_LIBRARY_PATH': bin_path}

        # # custom: run test
        # with tools.environment_append(vars):
        #     if platform.system() == "Windows":
        #         command = "tesseract.plugin.test"
        #     else:
        #         command = "./tesseract.plugin.test"
        #     self.run(
        #         command,
        #         cwd="%stest" %
        #         self.deps_cpp_info["tesseract.plugin"].build_paths[0])
