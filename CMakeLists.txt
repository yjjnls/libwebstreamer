cmake_minimum_required(VERSION 2.8)
project(libwebstreamer)


enable_testing()
include(${CMAKE_CURRENT_SOURCE_DIR}/conan.cmake)
conan_find_pkgconfig( 0.29.1 )
conan_compiler_flags()
pkg_check_modules(GST_MODULES  REQUIRED                  
                  gstreamer-1.0>=1.14.0
                  gstreamer-base-1.0>=1.14.0
                  gstreamer-rtsp-server-1.0
                  gstreamer-sdp-1.0
                  gstreamer-webrtc-1.0
                  gstreamer-video-1.0 )

include_directories(${GST_MODULES_INCLUDE_DIRS}) 
link_directories   (${GST_MODULES_LIBRARY_DIRS})

add_subdirectory( lib )
