
cmake_minimum_required ( VERSION 2.8 )

project ( ReaperGL C CXX )

list ( APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake )
find_package ( OpenGL REQUIRED )
find_package ( GLEW REQUIRED )
find_package ( GLFW REQUIRED )
find_package ( CXX11 REQUIRED )

include_directories ( ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS} ${GLFW_INCLUDE_DIR} )
include_directories ( include src )

set ( CMAKE_CXX_FLAGS "-Wall -Wextra -ansi -pedantic ${CXX11_FLAGS} ${CMAKE_CXX_FLAGS}" )

add_subdirectory ( src )
