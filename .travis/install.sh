#! /bin/bash
sudo conan remote add upload_stable https://api.bintray.com/conan/${DEPENDENT_BINTRAY_REPO}/stable --insert 0 >/dev/null

yes|sudo add-apt-repository ppa:ubuntu-toolchain-r/test
yes|sudo apt-get update
sudo apt-get install gcc-4.9
sudo apt-get install g++-4.9
sudo rm -f /usr/bin/gcc /usr/bin/g++
sudo ln -s /usr/bin/gcc-4.9 /usr/bin/gcc
sudo ln -s /usr/bin/g++-4.9 /usr/bin/g++

pushd /tmp
wget https://cmake.org/files/v3.11/cmake-3.11.3-Linux-x86_64.tar.gz
tar -xf cmake-3.11.3-Linux-x86_64.tar.gz
sudo cp -rf cmake-3.11.3-Linux-x86_64/bin /usr/local
sudo cp -rf cmake-3.11.3-Linux-x86_64/share /usr/local

wget https://pkg-config.freedesktop.org/releases/pkg-config-0.29.1.tar.gz
tar -zxf pkg-config-0.29.1.tar.gz
pushd pkg-config-0.29.1
./configure --prefix=/usr --with-internal-glib --disable-host-tool --docdir=/usr/share/doc/pkg-config-0.29.1
make
sudo make install

popd
popd