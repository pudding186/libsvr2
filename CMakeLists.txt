cmake_minimum_required (VERSION 2.80)

project (libsvr2)

aux_source_directory (src SRC_LIST)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
list(REMOVE_ITEM SRC_LIST "src/iocp_tcp.c")
endif()

#if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -mcx16")
#endif ()

#if (CMAKE_C_COMPILER_ID MATCHES "GNU")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -mcx16")
#endif ()

if(MSVC)
set(
    VCPKG_PATH
    "D:/vcpkg"
)

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
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
set(
    VCPKG_PATH
    "/root/work/vcpkg"
)

include_directories(
	include
	${VCPKG_PATH}/installed/x64-linux/include
	)
	
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic -std=gnu++11 -mcx16")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-long-long -pedantic -std=gnu11 -mcx16")
endif()


add_library(svr2 STATIC ${SRC_LIST})

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
