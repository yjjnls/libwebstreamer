#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import absolute_import, division, print_function
import sys
import os
import re
import platform
import shutil
import traceback

from shell import shell as call
from cpt.packager import ConanMultiPackager
from conans.client.loader_parse import load_conanfile_class
__dir__ = os.path.dirname(os.path.abspath(__file__))


def get_build_number():
    '''
    Get current git repo commits time since last tag.
    if no commit, this is an tag
    '''
    commitid = call('git rev-list --tags --no-walk --max-count=1')
    if (not commitid.output()):
        commitid = call('git rev-list HEAD --max-count=1')
        count = call('git rev-list  %s --count' % commitid.output()[0])
    else:
        count = call('git rev-list  %s.. --count' % commitid.output()[0])

    return int(count.output()[0])


def update_version(version):
    # conanfile.py
    f = open(os.path.join(__dir__, 'conanfile.py'), 'rt')
    content = f.read()
    f.close()  #
    P_VERSION = re.compile(r'''version\s*=\s*["'](?P<version>\S*)["']''')

    def _replace(m):
        return 'version = "%s"' % version

    content = P_VERSION.sub(_replace, content)
    f = open(os.path.join(__dir__, 'conanfile.py'), 'wb')
    f.write(content)
    f.close()


def build():
    n = get_build_number()
    conanfile = load_conanfile_class(os.path.join(__dir__, 'conanfile.py'))
    version = conanfile.version
    CONAN_STABLE_CHANNEL = None

    if n == 0:
        CONAN_CHANNEL = 'stable'
        CONAN_UPLOAD_ONLY_WHEN_STABLE = True
    else:
        version = '%s.%d' % (version, n)
        CONAN_CHANNEL = 'testing'
        CONAN_UPLOAD_ONLY_WHEN_STABLE = False
        CONAN_STABLE_CHANNEL = 'testing'
        update_version(version)

    CONAN_UPLOAD = 'https://api.bintray.com/conan/%s/%s' % (CONAN_USERNAME,
                                                            CONAN_CHANNEL)

    builder = ConanMultiPackager(
        channel=CONAN_CHANNEL,
        upload_only_when_stable=CONAN_UPLOAD_ONLY_WHEN_STABLE,
        upload=CONAN_UPLOAD,
        username=CONAN_USERNAME,
        stable_channel=CONAN_STABLE_CHANNEL)

    builder.add_common_builds()
    builds = []
    for settings, options, env_vars, build_requires, reference in builder.items:
        # dynamic only
        if not options["libwebstreamer:shared"]:
            continue
        # release only
        if settings["build_type"] == "Debug":
            continue

        # Visual Sutido 2017 only
        if platform.system() == "Windows":
            if settings["compiler"] == "Visual Studio":
                if settings["compiler.version"] == '14':
                    builds.append(
                        [settings, options, env_vars, build_requires])
        elif platform.system() == "Linux":
            if settings["compiler"] == "gcc":
                if settings["compiler.version"] == '4.9' and settings["arch"] == 'x86_64':
                    builds.append(
                        [settings, options, env_vars, build_requires])
    builder.builds = builds
    builder.run()


if __name__ == '__main__':
    '''
    windows release x86
    set CONAN_VISUAL_VERSIONS=15
    set CONAN_BUILD_TYPES=Release
    set CONAN_ARCHS=x86
    python build.py

    '''
    os.rename('conanfile.py', 'conanfile.py.origin~')
    shutil.copy('conanfile.py.origin~', 'conanfile.py')

    CONAN_USERNAME = os.environ.get("CONAN_USERNAME", "yjjnls")
    os.environ['CONAN_USERNAME'] = CONAN_USERNAME

    try:
        if os.path.exists('conanfile.py'):
            build()
    except:
        traceback.print_exc()
    finally:
        os.remove('conanfile.py')
        os.rename('conanfile.py.origin~', 'conanfile.py')
