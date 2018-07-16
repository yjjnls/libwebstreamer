# libwebstreamer

Coding Rule see [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)

# How to build

## Linux

[![](https://www.travis-ci.org/yjjnls/libwebstreamer.svg?branch=master)](https://www.travis-ci.org/yjjnls/libwebstreamer)  

libwebstreamer coulld be built in either docker or your host. The release versions are all built in docker `lasote/conangcc49` by travis.

### Prework you have to do firstly

1.  Update source

```sh
$ sudo apt-get -y update
$ sudo apt-get -y upgrade
```

2.  Config git

```sh
$ git config --global user.name "xxx"
$ git config --global user.email "xxxx"
```

3.  Install required softwares. If you have installed, ignore it.

```sh
## conan module
$ sudo pip install conan --upgrade
$ sudo pip install conan_package_tools bincrafters_package_tools
$ sudo pip install shell
$ sudo conan user

## pkg-config 0.29.1
$ wget https://pkg-config.freedesktop.org/releases/pkg-config-0.29.1.tar.gz
$ tar -zxf pkg-config-0.29.1.tar.gz
$ cd pkg-config-0.29.1
$ ./configure --prefix=/usr        \
            --with-internal-glib \
            --disable-host-tool  \
            --docdir=/usr/share/doc/pkg-config-0.29.1
$ make
$ sudo make install
$ cd -

## node (at least 10.4.0)
$ wget https://nodejs.org/dist/v10.4.0/node-v10.4.0-linux-x64.tar.xz
$ tar -xf node-v10.4.0-linux-x64.tar.xz
$ sudo cp -rf node-v10.4.0-linux-x64/* /usr/local

## python3.5 (for test)
$ sudo apt-get install -y python3.5 python3.5-dev python3-pip
$ sudo rm -f /usr/bin/python3
$ sudo ln -s /usr/bin/python3.5 /usr/bin/python3
$ sudo pip3 install websockets
```

### Start to build
-   Dependencies

1.  gstreamer>=1.14.0

    You can build gstreamer yourself using the [repo](https://github.com/yjjnls/conan-gstreamer.git) and upload the built packages to bintray for the next use.  

    Or you can just install the pre-built packages from other bintray repo, hence, `DEPENDENT_BINTRAY_REPO` should be set in this method, it's the username of the bintray repo you'll use. Moreover, some extra packages installed in the bootstrap step in building gstreamer will be used in the test, and they're not included in the pre-built packages, so you should install them individually.

    The gstreamer libs will be installed in the dir `~/gstreamer/linux_x86_64`.

2.  tesseract
    you can still use the repos [tesseract.conan](https://github.com/yjjnls/tesseract.conan.git) and [tesseract.plugin](https://github.com/yjjnls/tesseract.plugin.git) to build your own packages or use the pre-built packages. Methods are the same as gstreamer.

-   Start build

```sh
$ sudo python build.py
```
