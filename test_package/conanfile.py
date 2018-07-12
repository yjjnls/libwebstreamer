#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import platform

from conans import CMake, ConanFile, RunEnvironment, tools


class TestPackageConan(ConanFile):
    def system_requirements(self):
        if platform.system() == "Linux" and os.environ.get('CONAN_DOCKER_IMAGE'):
            self.run(
                "wget https://nodejs.org/dist/v10.4.0/node-v10.4.0-linux-x64.tar.xz")
            self.run("tar -xf node-v10.4.0-linux-x64.tar.xz")
            self.run("sudo cp -rf node-v10.4.0-linux-x64/* /usr/local")

    def build_requirements(self):
        self.build_requires("tesseract.plugin/0.2.2@%s/stable" %
                            os.environ.get("CONAN_USERNAME", "yjjnls"))

    def test(self):
        self.run("git clone https://github.com/yjjnls/webstreamer.git")
        self.run("cd webstreamer && git checkout linux")
        # just use sudo npm install will raise a bug
        # https://github.com/gdotdesign/elm-github-install/issues/21
        self.run("npm install --unsafe-perm=true --allow-root",
                 cwd="%s/webstreamer" % os.getcwd())

        self.run("git clone https://github.com/yjjnls/gstwebrtc-demos.git")
        self.run(
            "pid=$(ps -elf |grep -i simple-server|grep -v grep|awk '{print $4}') && sudo kill -9 $pid")
        self.run("cd gstwebrtc-demos/signalling && python3 simple-server.py&")

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
            'LD_LIBRARY_PATH': '%s/lib:%s:%s' % (gstreamer_root, libwebstreamer_path, bin_path),
            'TESSERACT_PLUGIN_PATH': tesseract_plugin_path,
            'LIBWEBSTREAMER_PATH': libwebstreamer_path,
            'TESSDATA_PREFIX': tessdata_dir,
            'GST_DEBUG': 'rtsp*:3,webstreamer:5,*:2',
            'GSTREAMER_ROOT': gstreamer_root,
            'GST_PLUGIN_SYSTEM_PATH_1_0': '%s/lib/gstreamer-1.0' % gstreamer_root,
            'GST_PLUGIN_SCANNER_1_0': '%s/libexec/gstreamer-1.0/gst-plugin-scanner' % gstreamer_root,
            'PATH': '%s/bin:%s' % (gstreamer_root, os.environ['PATH']),
            'GI_TYPELIB_PATH': '%s/lib/girepository-1.0' % gstreamer_root
        }
        print ''
        print 'export LD_LIBRARY_PATH=%s/lib:%s/lib/gstreamer-1.0:%s/bin:%s:%s' % (
            gstreamer_root, gstreamer_root, gstreamer_root, libwebstreamer_path, bin_path)
        print 'export TESSERACT_PLUGIN_PATH=%s' % tesseract_plugin_path
        print 'export LIBWEBSTREAMER_PATH=%s' % libwebstreamer_path
        print 'export TESSDATA_PREFIX=%s' % tessdata_dir
        print 'export GSTREAMER_ROOT=%s' % gstreamer_root
        print 'export GST_DEBUG=*:2,rtspclient:5,webstreamer:5'
        print 'export GST_PLUGIN_SYSTEM_PATH_1_0=%s/lib/gstreamer-1.0' % gstreamer_root
        print 'export GST_PLUGIN_SCANNER_1_0=%s/libexec/gstreamer-1.0/gst-plugin-scanner' % gstreamer_root
        print 'export PATH=%s/bin:%s' % (gstreamer_root, os.environ['PATH']),
        print 'export GI_TYPELIB_PATH=%s/lib/girepository-1.0' % gstreamer_root
        print ''

        # custom: run test
        with tools.environment_append(vars):
            if platform.system() == "Windows":
                # command = "tesseract.plugin.test"
                pass
            else:
                command1 = "cd %s/bin && ./gst-inspect-1.0 && cd - &&" % gstreamer_root
                command2 = "npm test test/livestream.test.js "
            self.run("%s%s" % (command1, command2),
                     cwd="%s/webstreamer" % os.getcwd())
