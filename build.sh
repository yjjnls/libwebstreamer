#! /bin/bash
# rm -rf build

mkdir -p build
pushd build
cmake ..
make 
make install
popd