
#
# group source files in Visual Studio
#
macro(conan_project_group source_files sgbd_cur_dir)
    if(MSVC)
        foreach(sgbd_file ${${source_files}})

            string(REGEX REPLACE ${sgbd_cur_dir}/\(.*\) \\1 sgbd_fpath ${sgbd_file})
            string(REGEX REPLACE "\(.*\)/.*" \\1 sgbd_group_name ${sgbd_fpath})
            string(COMPARE EQUAL ${sgbd_fpath} ${sgbd_group_name} sgbd_nogroup)
            string(REPLACE "/" "\\" sgbd_group_name ${sgbd_group_name})

            if(sgbd_nogroup)
                set(sgbd_group_name "\\")
            endif(sgbd_nogroup)

            source_group(${sgbd_group_name} FILES ${sgbd_file})

        endforeach(sgbd_file)
    endif()
endmacro(conan_project_group)

#
#
# find pkg-config, if not found try download it
#
set(CAM_TOOLS_URL https://github.com/Mingyiz/cam/releases/download/tools )

macro(conan_find_pkgconfig version )
    find_package(PkgConfig ${version} )
    if( NOT PKG_CONFIG_FOUND )
        set( _version 0.29.2)
        
        if( MSVC )
           set( _basename "pkg-config-${_version}-windows")
           set( _tarname  "${_basename}.tar.gz")
           set( _hash e247a276562398946507abde0a8bcb1f)
           set( _url ${CAM_TOOLS_URL}/${_tarname})
           set( _filename "${CMAKE_CURRENT_BINARY_DIR}/build-tools/.cache/${_tarname}")
           
           message(STATUS "Downloading pkg-config binary ...")
           file( DOWNLOAD ${_url} ${_filename}
           SHOW_PROGRESS
           INACTIVITY_TIMEOUT 60 # seconds
           EXPECTED_HASH MD5=${_hash} )

           execute_process(COMMAND ${CMAKE_COMMAND} -E tar xz "${_filename}"
           WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
           RESULT_VARIABLE __result )

           if(NOT __result EQUAL 0)
               message(FATAL_ERROR "error ${__result}")
           endif()

           set(PKG_CONFIG_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/${_basename}/pkg-config")

        endif()
    endif()
    find_package(PkgConfig ${version} REQUIRED )
endmacro(conan_find_pkgconfig)

#
# gstreamer helper
#
# macro(conan_find_gstreamer version )
#     pkg_check_modules(_GSTREAMER QUIET gstreamer-1.0>=${version})
#     if( NOT _GSTREAMER_NOT_FOUND)
#         if( CAM_TARGET_ARCH STREQUAL x86  )

#             if( $ENV{GSTREAMER_1_0_ROOT_X86})
#                set( _pkgconfigdir "${GSTREAMER_1_0_ROOT_X86}/lib/pkgconfig")
#             else()
#                set( _pkgconfigdir "c:/gstreamer/1.0/x86/lib/pkgconfig")               
#             endif()

#         elseif( CAM_TARGET_ARCH STREQUAL x86_64)

#             if( $ENV{GSTREAMER_1_0_ROOT_X86_64})
#                set( _pkgconfigdir "${GSTREAMER_1_0_ROOT_X86_64}/lib/pkgconfig")
#             else()
#                set( _pkgconfigdir "c:/gstreamer/1.0/x86_64/lib/pkgconfig")               
#             endif()
#         else()
#            message(FATAL_ERROR "unknow target arch ${CAM_TARGET_ARCH} at ${CMAKE_GENERATOR}")
#         endif()
#         file(TO_CMAKE_PATH "${_pkgconfigdir}" _pkgconfigdir)

#         set(_sep ":")
#         if( MSVC )
#             set( _sep ";")
#         endif()
        
#         set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}${_sep}${_pkgconfigdir}")
        
#         pkg_check_modules(_GSTREAMER REQUIRED gstreamer-1.0>=${version})
#     endif()
# endmacro(conan_find_gstreamer)

#
# compiler flags
#
macro(conan_compiler_flags )
    if(MSVC)
    	ADD_DEFINITIONS( -D_CRT_SECURE_NO_DEPRECATE  )
    	ADD_DEFINITIONS( -D_CRT_NONSTDC_NO_DEPRECATE )
    	ADD_DEFINITIONS( -D_SCL_SECURE_NO_WARNINGS   )
    	
    	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4819")
    else()
    	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")    	
    endif()
endmacro(conan_compiler_flags)

