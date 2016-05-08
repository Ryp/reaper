cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(Reaper)

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are discouraged. Make a separate folder for building:\nmkdir build; cd build; cmake ..;cd ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif()

# Provide custom cmake scripts
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(cotire)
include(ReaperGL)

if (WIN32)
    set(VK_PLATFORM "VK_USE_PLATFORM_WIN32_KHR")
elseif(APPLE OR UNIX)
    set(VK_PLATFORM "VK_USE_PLATFORM_XCB_KHR")
    find_package(XCB REQUIRED)
    set(REAPER_PLATFORM_LIBRARIES ${REAPER_PLATFORM_LIBRARIES} ${LIBXCB_LIBRARIES})
    set(REAPER_PLATFORM_INCLUDES ${REAPER_PLATFORM_INCLUDES} ${LIBXCB_INCLUDE_DIR})
endif()

# Find dependencies
find_package(moGL NO_MODULE REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glbinding REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(assimp REQUIRED)
find_package(Libunwind REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
find_package(Vulkan REQUIRED)

set(REAPER_BIN reaper)

set(REAPER_SRCS
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_impl_glfw_gl3.cpp

    ${CMAKE_SOURCE_DIR}/src/main.cpp

    ${CMAKE_SOURCE_DIR}/src/core/Assert.h
    ${CMAKE_SOURCE_DIR}/src/core/BitTricks.h
    ${CMAKE_SOURCE_DIR}/src/core/Cast.h
    ${CMAKE_SOURCE_DIR}/src/core/Compiler.h
    ${CMAKE_SOURCE_DIR}/src/core/Debugger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Debugger.h
    ${CMAKE_SOURCE_DIR}/src/core/DynamicLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DynamicLibrary.h
    ${CMAKE_SOURCE_DIR}/src/core/EnumHelper.h
    ${CMAKE_SOURCE_DIR}/src/core/Platform.h
    ${CMAKE_SOURCE_DIR}/src/core/StackTrace.cpp
    ${CMAKE_SOURCE_DIR}/src/core/StackTrace.h
    ${CMAKE_SOURCE_DIR}/src/core/Types.h

    ${CMAKE_SOURCE_DIR}/src/core/fs/FileLoading.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fs/FileLoading.h
    ${CMAKE_SOURCE_DIR}/src/core/fs/Path.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fs/Path.h
    ${CMAKE_SOURCE_DIR}/src/core/fs/UnixFileWatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fs/UnixFileWatcher.h

    ${CMAKE_SOURCE_DIR}/src/core/memory/Allocator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/memory/Allocator.h
    ${CMAKE_SOURCE_DIR}/src/core/memory/CacheLine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/memory/CacheLine.h
    ${CMAKE_SOURCE_DIR}/src/core/memory/LinearAllocator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/memory/LinearAllocator.h
    ${CMAKE_SOURCE_DIR}/src/core/memory/StackAllocator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/memory/StackAllocator.h

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

    ${CMAKE_SOURCE_DIR}/src/input/AbstractController.cpp
    ${CMAKE_SOURCE_DIR}/src/input/AbstractController.h
    ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.cpp
    ${CMAKE_SOURCE_DIR}/src/input/SixAxisController.h

    ${CMAKE_SOURCE_DIR}/src/math/BasicMath.h
    ${CMAKE_SOURCE_DIR}/src/math/Spline.cpp
    ${CMAKE_SOURCE_DIR}/src/math/Spline.h

    ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h

    ${CMAKE_SOURCE_DIR}/src/renderer/Camera.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/Camera.h
    ${CMAKE_SOURCE_DIR}/src/renderer/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/Renderer.h

    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/ImageLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/ImageLoader.h
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/Mesh.h
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/OpenGLRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/OpenGLRenderer.h
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/ShaderHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/ShaderHelper.h

    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/api/OpenGLContext.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/api/OpenGLContext.h
    ${CMAKE_SOURCE_DIR}/src/renderer/opengl/api/OpenGL.h

    ${CMAKE_SOURCE_DIR}/src/renderer/resource/Model.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/resource/Model.h
    ${CMAKE_SOURCE_DIR}/src/renderer/resource/ModelLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/resource/ModelLoader.h
    ${CMAKE_SOURCE_DIR}/src/renderer/resource/ResourceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/resource/ResourceManager.h

    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/PresentationSurface.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/PresentationSurface.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/VulkanRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/VulkanRenderer.h

    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/api/Vulkan.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/api/VulkanFunctions.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/api/VulkanFunctions.h
    ${CMAKE_SOURCE_DIR}/src/renderer/vulkan/api/VulkanSymbolHelper.inl
)

# Set precompiled header
set(REAPER_PCH ${CMAKE_SOURCE_DIR}/src/pch/stdafx.h)

add_executable(${REAPER_BIN} ${REAPER_SRCS})
add_custom_build_flags(${REAPER_BIN} "ReaperGL")

set_target_properties(${REAPER_BIN} PROPERTIES COMPILE_DEFINITIONS ${VK_PLATFORM})

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
    ${OPENEXR_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIR}
    ${REAPER_PLATFORM_INCLUDES})

#Libraries
target_link_libraries(${REAPER_BIN}
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${GLBINDING_LIBRARY_RELEASE}
    ${ASSIMP_LIBRARIES}
    ${LIBUNWIND_LIBRARIES}
    ${OPENEXR_LIBRARIES}
    ${REAPER_PLATFORM_LIBRARIES})

set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
set_target_properties(${REAPER_BIN} PROPERTIES COTIRE_UNITY_LINK_LIBRARIES_INIT "COPY")
cotire(${REAPER_BIN})
