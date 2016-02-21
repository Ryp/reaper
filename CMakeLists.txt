cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(Reaper)

# Provide custom cmake scripts
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(cotire)

# Find dependencies
find_package(moGL NO_MODULE REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glbinding REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(assimp REQUIRED)
find_package(Libunwind REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

set(REAPER_BIN reaper)

set(REAPER_SRCS
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_impl_glfw_gl3.cpp
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h
    ${CMAKE_SOURCE_DIR}/src/core/Assert.h
    ${CMAKE_SOURCE_DIR}/src/core/Debugger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Debugger.h
    ${CMAKE_SOURCE_DIR}/src/core/Platform.h
    ${CMAKE_SOURCE_DIR}/src/core/StackTrace.cpp
    ${CMAKE_SOURCE_DIR}/src/core/StackTrace.h
    ${CMAKE_SOURCE_DIR}/src/Camera.cpp
    ${CMAKE_SOURCE_DIR}/src/cvar/cvar.cpp
    ${CMAKE_SOURCE_DIR}/src/unixfilewatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/filesystem/path.cpp
    ${CMAKE_SOURCE_DIR}/src/input/AController.cpp
    ${CMAKE_SOURCE_DIR}/src/input/SixAxis.cpp
    ${CMAKE_SOURCE_DIR}/src/math/spline.cpp
    ${CMAKE_SOURCE_DIR}/src/pipeline/glcontext.cpp
    ${CMAKE_SOURCE_DIR}/src/pipeline/mesh.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/imageloader.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/Model.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/ModelLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/resourcemanager.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/shaderhelper.cpp
)

# Set precompiled header
set(REAPER_PCH ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h)

add_executable(${REAPER_BIN} ${REAPER_SRCS})

#Set compilation flags
set(REAPER_COMPILE_FLAGS "${CMAKE_CXX_FLAGS}")
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(REAPER_COMPILE_FLAGS "${REAPER_COMPILE_FLAGS} -Wall -Wextra -Wshadow -Wundef")
endif()
set_target_properties(${REAPER_BIN} PROPERTIES
    CXX_STANDARD 14
    COMPILE_FLAGS "${REAPER_COMPILE_FLAGS}")

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_BIN} SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/external
    ${OPENGL_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${MOGL_INCLUDE_DIRS}
    ${GLBINDING_INCLUDES}
    ${ASSIMP_INCLUDE_DIRS}
    ${LIBUNWIND_INCLUDE_DIR}
    ${OPENEXR_INCLUDE_DIRS})

#Libraries
target_link_libraries(${REAPER_BIN}
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${GLBINDING_LIBRARIES}
    ${ASSIMP_LIBRARIES}
    ${LIBUNWIND_LIBRARIES}
    ${OPENEXR_LIBRARIES} -lpthread)

set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_UNITY_LINK_LIBRARIES_INIT "COPY")
cotire(${REAPER_BIN})
