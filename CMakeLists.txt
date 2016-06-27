cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(Reaper)

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are discouraged. Make a separate folder for building:\nmkdir build; cd build; cmake ..;cd ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif()

# Provide custom cmake scripts
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(cotire)
include(Reaper)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})

option(REAPER_BUILD_KDEV_TEMPLATES      "Build custom templates for kdevelop" OFF)
option(REAPER_BUILD_TESTS               "Build tests" ON)
option(REAPER_BUILD_SHARED_LIBRARIES    "Build shared libraries" ON)

if (WIN32)
    set(VK_PLATFORM "VK_USE_PLATFORM_WIN32_KHR")
elseif(APPLE OR UNIX)
    set(VK_PLATFORM "VK_USE_PLATFORM_XCB_KHR")
    find_package(XCB REQUIRED)
    set(REAPER_PLATFORM_LIBRARIES ${REAPER_PLATFORM_LIBRARIES} ${LIBXCB_LIBRARIES})
    set(REAPER_PLATFORM_INCLUDES ${REAPER_PLATFORM_INCLUDES} ${LIBXCB_INCLUDE_DIR})
endif()

# Find dependencies
find_package(Vulkan REQUIRED)
find_package(assimp REQUIRED)

# Determine linkage type
if(REAPER_BUILD_SHARED_LIBRARIES)
    set(REAPER_BUILD_TYPE SHARED)
else()
    set(REAPER_BUILD_TYPE STATIC)
endif()

set(REAPER_BIN reaper)

# Set precompiled header
set(REAPER_PCH ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h)

add_subdirectory(src/core)

set(REAPER_SRCS
    ${CMAKE_SOURCE_DIR}/src/main.cpp

    ${CMAKE_SOURCE_DIR}/src/cvar/CVar.cpp
    ${CMAKE_SOURCE_DIR}/src/cvar/CVar.h

    ${CMAKE_SOURCE_DIR}/src/game/EntityDescriptor.h
    ${CMAKE_SOURCE_DIR}/src/game/ModuleUpdater.cpp
    ${CMAKE_SOURCE_DIR}/src/game/ModuleUpdater.h
    ${CMAKE_SOURCE_DIR}/src/game/WorldUpdater.cpp
    ${CMAKE_SOURCE_DIR}/src/game/WorldUpdater.h

    ${CMAKE_SOURCE_DIR}/src/game/entitydb/EntityDb.cpp
    ${CMAKE_SOURCE_DIR}/src/game/entitydb/EntityDb.h

    ${CMAKE_SOURCE_DIR}/src/game/map/MapDescriptor.h
    ${CMAKE_SOURCE_DIR}/src/game/map/MapInfo.cpp
    ${CMAKE_SOURCE_DIR}/src/game/map/MapInfo.h

    ${CMAKE_SOURCE_DIR}/src/game/module/DamageModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/DamageModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/MovementModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/MovementModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/PathModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/PathModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/PositionModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/PositionModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/TeamModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/TeamModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/TowerControllerModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/TowerControllerModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/WaveSpawnerModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/WaveSpawnerModule.h
    ${CMAKE_SOURCE_DIR}/src/game/module/WeaponModule.cpp
    ${CMAKE_SOURCE_DIR}/src/game/module/WeaponModule.h

    ${CMAKE_SOURCE_DIR}/src/game/pathing/AIPath.cpp
    ${CMAKE_SOURCE_DIR}/src/game/pathing/AIPath.h
    ${CMAKE_SOURCE_DIR}/src/game/pathing/BreadthFirstSearch.cpp
    ${CMAKE_SOURCE_DIR}/src/game/pathing/BreadthFirstSearch.h
    ${CMAKE_SOURCE_DIR}/src/game/pathing/CostMap.cpp
    ${CMAKE_SOURCE_DIR}/src/game/pathing/CostMap.h
#     ${CMAKE_SOURCE_DIR}/src/game/pathing/PathFinder.cpp
#     ${CMAKE_SOURCE_DIR}/src/game/pathing/PathFinder.h

#     ${CMAKE_SOURCE_DIR}/src/input/AbstractController.cpp
#     ${CMAKE_SOURCE_DIR}/src/input/AbstractController.h
#     ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.cpp
#     ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.h

    ${CMAKE_SOURCE_DIR}/src/math/BasicMath.h
    ${CMAKE_SOURCE_DIR}/src/math/Spline.cpp
    ${CMAKE_SOURCE_DIR}/src/math/Spline.h

    ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h

    ${CMAKE_SOURCE_DIR}/src/renderer/Camera.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/Camera.h
    ${CMAKE_SOURCE_DIR}/src/renderer/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/Renderer.h

    ${CMAKE_SOURCE_DIR}/src/renderer/format/FloatTypes.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/format/FloatTypes.h
    ${CMAKE_SOURCE_DIR}/src/renderer/format/PixelFormat.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/format/PixelFormat.h

    ${CMAKE_SOURCE_DIR}/src/renderer/mesh/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/mesh/Mesh.h
    ${CMAKE_SOURCE_DIR}/src/renderer/mesh/MeshCache.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/mesh/MeshCache.h
    ${CMAKE_SOURCE_DIR}/src/renderer/mesh/ModelLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/mesh/ModelLoader.h

    ${CMAKE_SOURCE_DIR}/src/renderer/texture/Texture.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/texture/Texture.h
    ${CMAKE_SOURCE_DIR}/src/renderer/texture/TextureCache.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/texture/TextureCache.h
    ${CMAKE_SOURCE_DIR}/src/renderer/texture/TextureLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/texture/TextureLoader.h

    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/Memory.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/Memory.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/PresentationSurface.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/PresentationSurface.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/SwapchainRendererBase.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/SwapchainRendererBase.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/Shader.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/Shader.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/VulkanRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/VulkanRenderer.h

    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/api/Vulkan.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/api/VulkanFunctions.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/api/VulkanFunctions.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/api/VulkanSymbolHelper.inl
)

set(REAPER_GLSL_SHADERS
    ${CMAKE_SOURCE_DIR}/res/shader/tri.frag
    ${CMAKE_SOURCE_DIR}/res/shader/tri.vert
    ${CMAKE_SOURCE_DIR}/res/shader/push_constants.frag
    ${CMAKE_SOURCE_DIR}/res/shader/push_constants.vert
    ${CMAKE_SOURCE_DIR}/res/shader/uniform.frag
    ${CMAKE_SOURCE_DIR}/res/shader/uniform.vert
)

add_glslang_spirv_targets(REAPER_SPIRV_OUTPUT ${REAPER_GLSL_SHADERS})

add_executable(${REAPER_BIN} ${REAPER_SRCS} ${REAPER_GLSL_SHADERS} ${REAPER_SPIRV_OUTPUT})
add_custom_build_flags(${REAPER_BIN} "Reaper")

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${REAPER_BIN})

set_target_properties(${REAPER_BIN} PROPERTIES COMPILE_DEFINITIONS ${VK_PLATFORM})

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_BIN} SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/external
    ${VULKAN_INCLUDE_DIR}
    ${ASSIMP_INCLUDE_DIRS}
    ${REAPER_PLATFORM_INCLUDES})

#Libraries
target_link_libraries(${REAPER_BIN}
    ${ASSIMP_LIBRARY}
    ${REAPER_PLATFORM_LIBRARIES}
    ${REAPER_CORE_BIN})

# Copy dlls to binary dir
if (WIN32)
    set(ASSIMP_SHARED_LIBRARY_RELEASE ${ASSIMP_LIBRARY_DIR}/assimp-${ASSIMP_MSVC_VERSION}-mt.dll)
    set(ASSIMP_SHARED_LIBRARY_DEBUG   ${ASSIMP_LIBRARY_DIR}/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll)
    file(COPY ${ASSIMP_SHARED_LIBRARY_DEBUG}   DESTINATION ${CMAKE_BINARY_DIR}/Debug NO_SOURCE_PERMISSIONS)
    file(COPY ${ASSIMP_SHARED_LIBRARY_RELEASE} DESTINATION ${CMAKE_BINARY_DIR}/Release NO_SOURCE_PERMISSIONS)
endif()

set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_BIN})

if (REAPER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if (REAPER_BUILD_KDEV_TEMPLATES)
    add_subdirectory(tools/kdevelop)
endif()
