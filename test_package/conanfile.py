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
        self.build_requires("tesseract.plugin/0.4.0@%s/stable" %
                            os.environ.get("DEPENDENT_BINTRAY_REPO"))

    def test(self):
        if platform.system() == "Linux" and os.environ.get('CONAN_DOCKER_IMAGE'):
            self.run("sudo apt-get -y update")
            self.run("sudo apt-get -y upgrade")
            self.run("sudo apt-get install -y python3.5 python3.5-dev python3-pip")
            self.run(
                "sudo rm -f /usr/bin/python3 &&sudo ln -s /usr/bin/python3.5 /usr/bin/python3")
            self.run("sudo pip3 install websockets")

            tool = 'sudo apt-get install -y '
            packages = ['libxv-dev', 'libx11-dev', 'libpulse-dev',
                        'libxext-dev', 'libxi-dev',
                        'libxrender-dev', 'libgl1-mesa-dev', 'libxfixes-dev',
                        'libxdamage-dev', 'libxcomposite-dev', 'libasound2-dev',
                        'transfig', 'libdbus-glib-1-dev', 'libxtst-dev',
                        'libxrandr-dev', 'libglu1-mesa-dev', 'libegl1-mesa-dev']
            for package in packages:
                self.run("%s%s" % (tool, package))

        self.run("git clone https://github.com/yjjnls/webstreamer.git")
        self.run("cd webstreamer && git checkout 0.1.0")
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
            'GST_DEBUG': 'rtsp*:3,webstreamer:5,*:1',
            'GSTREAMER_ROOT': gstreamer_root,
            'GST_PLUGIN_SYSTEM_PATH_1_0': '%s/lib/gstreamer-1.0' % gstreamer_root,
            'GST_PLUGIN_SCANNER_1_0': '%s/libexec/gstreamer-1.0/gst-plugin-scanner' % gstreamer_root,
            'PATH': '%s/bin:%s' % (gstreamer_root, os.environ['PATH']),
            'GI_TYPELIB_PATH': '%s/lib/girepository-1.0' % gstreamer_root
        }
        print '=================linux environment=================\n'
        print 'export LD_LIBRARY_PATH=%s/lib:%s:%s' % (
            gstreamer_root, libwebstreamer_path, bin_path)
        print 'export TESSERACT_PLUGIN_PATH=%s' % tesseract_plugin_path
        print 'export LIBWEBSTREAMER_PATH=%s' % libwebstreamer_path
        print 'export TESSDATA_PREFIX=%s' % tessdata_dir
        print 'export GST_DEBUG=rtsp*:3,webstreamer:5,*:1'
        print 'export GSTREAMER_ROOT=%s' % gstreamer_root
        print 'export GST_PLUGIN_SYSTEM_PATH_1_0=%s/lib/gstreamer-1.0' % gstreamer_root
        print 'export GST_PLUGIN_SCANNER_1_0=%s/libexec/gstreamer-1.0/gst-plugin-scanner' % gstreamer_root
        print 'export PATH=%s/bin:%s' % (gstreamer_root, os.environ['PATH']),
        print 'export GI_TYPELIB_PATH=%s/lib/girepository-1.0' % gstreamer_root
        print ''

        # custom: run test
        with tools.environment_append(vars):
            self.run("npm test test/livestream.test.js",
                     cwd="%s/webstreamer" % os.getcwd())
