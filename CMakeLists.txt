cmake_minimum_required (VERSION 2.80)

project (libsvr2)

aux_source_directory (src SRC_LIST)

include_directories(include)

add_library(svr2 STATIC ${SRC_LIST})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -mcx16")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -mcx16")
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
