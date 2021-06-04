cmake_minimum_required (VERSION 2.80)

project (libsvr)

aux_source_directory (src SRC_LIST)

#if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  # using Clang
#elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  # using GCC
#elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
#elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # using Visual Studio C++
#endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  if(MSVC)
    set(CMAKE_COMPILER_IS_CLANGCLXX 1)
  else()
    set(CMAKE_COMPILER_IS_CLANGXX 1)
  endif()
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  if(MSVC)
    set(CMAKE_COMPILER_IS_CLANGCL 1)
  else()
    set(CMAKE_COMPILER_IS_CLANG 1)
  endif()
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
list(REMOVE_ITEM SRC_LIST "src/iocp_tcp.c")
endif()

set(
    VCPKG_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../vcpkg"
)

if(MSVC)

include_directories(
	include
	${VCPKG_PATH}/installed/x64-windows-static/include
	)
	
# Use the static C library for all build types
    foreach(var 
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
      )
      if(${var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
      endif()
    endforeach()

  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
  
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcx16")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_CLANGXX)

    include_directories(
	include
	${VCPKG_PATH}/installed/x64-linux/include
	)
	
    # Update if necessary
    MESSAGE(STATUS "CMAKE_CXX_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.9)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -std=gnu++11")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-long-long -std=gnu11")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -std=gnu++11 -mcx16")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-long-long -std=gnu11 -mcx16")
    endif()
    MESSAGE(STATUS "CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
    MESSAGE(STATUS "CMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
endif()


add_library(libsvr STATIC ${SRC_LIST})

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../../bin)
