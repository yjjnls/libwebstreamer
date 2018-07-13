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
                            os.environ.get("CONAN_PREBUILT_REPO"))

    def test(self):
        if platform.system() == "Linux" and os.environ.get('CONAN_DOCKER_IMAGE'):
            self.run("sudo apt-get -y update")
            self.run("sudo apt-get -y upgrade")
            self.run("sudo apt-get install -y python3.5 python3.5-dev python3-pip")
            self.run(
                "sudo rm -f /usr/bin/python3 &&sudo ln -s /usr/bin/python3.5 /usr/bin/python3")
            self.run("sudo pip3 install websockets")
            # the packages below aren't all necessary, but the unnecessary ones are unkown. They're used for travis only
            self.run("sudo apt-get install -y dconf-gsettings-backend dconf-service dctrl-tools devscripts diffstat distro-info-data docbook docbook-dsssl docbook-to-man docbook-xml docbook-xsl doxygen dput fakeroot flex fontconfig-config fonts-dejavu-core gawk ghostscript glib-networking glib-networking-common glib-networking-services gnome-common gperf gsettings-desktop-schemas gsfonts gtk-doc-tools hardening-includes intltool jade libapr1 libaprutil1 libarchive13 libasound2 libasound2-data libasyncns0 libavahi-client3 libavahi-common-data libavahi-common3 libboost-system1.54.0 libcups2 libcupsfilters1 libcupsimage2 libcurl3 libdconf1 libdrm-amdgpu1 libdrm-intel1 libdrm-nouveau2 libdrm-radeon1 libegl1-mesa libegl1-mesa-drivers libelf1 libelfg0 libfakeroot libflac8 libfontconfig1 libfreetype6 libfuse2 libgbm1 libgl1-mesa-dri libgl1-mesa-glx libglapi-mesa libglib2.0-bin libglu1-mesa libgs9 libgs9-common libijs-0.35 libjbig0 libjbig2dec0 libjpeg-turbo8 libjpeg8 liblcms2-2 libllvm3.4 liblzo2-2 libmirclient7 libmirclientplatform-mesa libmirprotobuf0 libnetpbm10 libnettle4 libogg0 libopenvg1-mesa libpaper-utils libpaper1 libpciaccess0 libpcrecpp0 libprotobuf-lite8 libprotobuf8 libproxy1 libpulse-mainloop-glib0 libpulse0 libserf-1-1 libsndfile1 libsp1c2 libsvn1 libtiff5 libtxc-dxtn-s2tc0 libvorbis0a libvorbisenc2 libwayland-client0 libwayland-cursor0 libwayland-egl1-mesa libwayland-server0 libwrap0 libx11-6 libx11-data libx11-doc libx11-xcb1 libxau6 libxcb-dri2-0 libxcb-dri3-0 libxcb-glx0 libxcb-present0 libxcb-randr0 libxcb-render0 libxcb-shape0 libxcb-sync1 libxcb-xfixes0 libxcb1 libxcomposite1 libxdamage1 libxdmcp6 libxext6 libxfixes3 libxi6 libxkbcommon0 libxpm4 libxrandr2 libxrender1 libxshmfence1 libxslt1.1 libxtst6 libxv1 libxxf86vm1 lintian netpbm patchutils poppler-data sgml-data sp strace subversion t1utils tcpd texinfo transfig wdiff x11-common xorg-sgml-doctools xsltproc yasm")
            # self.run("sudo apt-get install -y libX11-6 libxrender1 libpulse0 libasound2 libjbig0 libxv1 libxext6 libxdamage1 libxfixes3 libgl1-mesa-dri libgl1-mesa-glx libglapi-mesa libwayland-egl1-mesa")

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
