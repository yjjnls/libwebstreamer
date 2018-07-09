project(lib)
file(GLOB_RECURSE  LWS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.[ch] 
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
conan_project_group(LWS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR})

foreach( filename ${LWS_SOURCES} )
message(${filename})
endforeach()

if( MSVC )
	set(libname libwebstreamer)
else()
	set(libname webstreamer)
endif()


include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(${libname}  SHARED ${LWS_SOURCES} )
target_link_libraries(${libname} ${GST_MODULES_LIBRARIES} )


set(instd ${CMAKE_HOME_DIRECTORY}/bin)
if( MSVC )
	set(instd ${instd}/win )
else()
	set(instd ${instd}/linux )
	# set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -std=c++11")
endif()

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL AMD64 OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL x86_64 )
	set(instd "${instd}/x64")
else()
	set(instd "${instd}/x86")
endif()
install(TARGETS ${libname} RUNTIME DESTINATION ${instd}
							   LIBRARY DESTINATION ${instd})

set(npm npm)
if(MSVC)
	set(npm npm.cmd)
endif()
add_test(NAME npm-install
         COMMAND ${npm} install webstreamer -s
		 WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY})
 
add_test(NAME run-webstreamer-test
         COMMAND ${npm} test
		 WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/node_modules/webstreamer)

SET_TESTS_PROPERTIES(run-webstreamer-test
 PROPERTIES ENVIRONMENT "LIBWEBSTREAMER_PATH=${instd}")

ADD_DEFINITIONS( -DGST_USE_UNSTABLE_API  )
