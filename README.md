# libwebstreamer
web oriented multimedia backend library


# how to build


## prerequisites
  * flatc  
    make sure you have install flatc (>=1.8.0)

  * pkg-config  
    make sure you have install pkg-config (>=0.29.1)
  
  * GStreamer and OpenWebRTC and set enviroment  
    for example in windows  

    set PKG_CONFIG_LIBDIR=C:\cerbero\x86_64\lib\pkgconfig


    then check if all these installation match your requirment  
    ```bash
    $ pkg-config --version
    0.29.1

    $ pkg-config --modversion gstreamer-1.0

    1.12.4

    $ flatc --version
    
    flatc version 1.8.0 (Nov 20 2017)

    ```

## fetch repositores
git clone https://github.com/kedacomresearch/libwebstreamer.git --recursive

## update flatbuffers defination (optional)
if you update sheme of the flatbuffers in doc/*.fbs, do as follows

```
flatc -o lib/include/fbs --cpp doc/common.fbs --include-prefix fbs 
flatc -o test/schema -s doc/common.fbs --include-prefix fbs
```


## build

entry tmp build directory 
```bash
mkdir build
cd build
```
build for windows x64 debug version

```bash
cmake .. -G "Visual Studio 14 2015 Win64" -DCMAKE_INSTALL_PREFIX="out" 
cmake --build . --target ALL_BUILD --config Debug -- /p:Platform=x64  
cmake --build . --target INSTALL   --config Debug -- /p:Platform=x64  

```
or just run the **build.bat** in the root dir  

## test

load enviroment
```
test\env.bat
```
run test
```
node test\test.js
```

## Code Standards
This project follows [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).