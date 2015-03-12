cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
project(Reaper)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(CXX11 REQUIRED) #Use C++14

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX11_FLAGS}")

add_subdirectory(src)
