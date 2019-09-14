cmake_minimum_required (VERSION 2.80)

project (libsvr2)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_C_STANDARD 99)
SET(CMAKE_C_STANDARD_REQUIRED ON)

aux_source_directory (src SRC_LIST)

include_directories(include)

add_library(svr2 STATIC ${SRC_LIST})

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -pthread")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
