#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(BULLET_PATH ${CMAKE_SOURCE_DIR}/external/bullet3)

option(USE_GRAPHICAL_BENCHMARK OFF)
option(USE_SOFT_BODY_MULTI_BODY_DYNAMICS_WORLD OFF)
option(ENABLE_VHACD OFF)
option(BUILD_CPU_DEMOS OFF)
option(USE_GLUT OFF)
option(BUILD_ENET OFF)
option(BUILD_CLSOCKET OFF)
option(BUILD_EGL OFF)
option(BUILD_OPENGL3_DEMOS OFF)
option(BUILD_BULLET2_DEMOS OFF)
option(BUILD_EXTRAS OFF)
option(INSTALL_LIBS OFF)
option(BUILD_UNIT_TESTS OFF)
option(INSTALL_CMAKE_FILES OFF)

add_subdirectory(${BULLET_PATH} ${CMAKE_BINARY_DIR}/external/bullet)

# Wrap bullet for easier consumption
add_library(bullet INTERFACE)

target_include_directories(bullet SYSTEM INTERFACE ${BULLET_PATH}/src)
target_link_libraries(bullet INTERFACE BulletDynamics BulletCollision LinearMath)
