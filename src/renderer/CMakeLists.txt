if(APPLE OR UNIX)
    find_package(XCB REQUIRED)
    set(REAPER_RENDERER_LIBRARIES ${LIBXCB_LIBRARIES})
    set(REAPER_RENDERER_INCLUDES ${LIBXCB_INCLUDE_DIR})
endif()

# Find dependencies
find_package(Vulkan REQUIRED)
find_package(assimp REQUIRED)

if (WIN32)
    assimp_copy_binaries(${Reaper_BINARY_DIR})
endif()

set(REAPER_RENDERER_LIBRARIES ${REAPER_RENDERER_LIBRARIES}
    ${REAPER_CORE_BIN}
    ${REAPER_COMMON_BIN}
    ${ASSIMP_LIBRARY})

set(REAPER_RENDERER_INCLUDES ${REAPER_RENDERER_INCLUDES}
    ${CMAKE_SOURCE_DIR}/external/fmt
    ${CMAKE_SOURCE_DIR}/external/gli
    ${CMAKE_SOURCE_DIR}/external/glm
    ${ASSIMP_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIR})

set(REAPER_RENDERER_BIN reaper_renderer)
set(REAPER_RENDERER_BIN ${REAPER_RENDERER_BIN} PARENT_SCOPE)

set(REAPER_RENDERER_DIR ${CMAKE_SOURCE_DIR}/src/renderer)
set(REAPER_RENDERER_SRCS
    ${REAPER_RENDERER_DIR}/Camera.cpp
    ${REAPER_RENDERER_DIR}/Camera.h

    ${REAPER_RENDERER_DIR}/Material.cpp
    ${REAPER_RENDERER_DIR}/Material.h
    ${REAPER_RENDERER_DIR}/Renderer.cpp
    ${REAPER_RENDERER_DIR}/Renderer.h
    ${REAPER_RENDERER_DIR}/RendererExport.h

    ${REAPER_RENDERER_DIR}/Scene.cpp
    ${REAPER_RENDERER_DIR}/Scene.h

    ${REAPER_RENDERER_DIR}/format/FloatTypes.cpp
    ${REAPER_RENDERER_DIR}/format/FloatTypes.h
    ${REAPER_RENDERER_DIR}/format/PixelFormat.cpp
    ${REAPER_RENDERER_DIR}/format/PixelFormat.h

    ${REAPER_RENDERER_DIR}/mesh/Mesh.cpp
    ${REAPER_RENDERER_DIR}/mesh/Mesh.h
    ${REAPER_RENDERER_DIR}/mesh/MeshCache.cpp
    ${REAPER_RENDERER_DIR}/mesh/MeshCache.h
    ${REAPER_RENDERER_DIR}/mesh/ModelLoader.cpp
    ${REAPER_RENDERER_DIR}/mesh/ModelLoader.h

    ${REAPER_RENDERER_DIR}/texture/Texture.cpp
    ${REAPER_RENDERER_DIR}/texture/Texture.h
    ${REAPER_RENDERER_DIR}/texture/TextureCache.cpp
    ${REAPER_RENDERER_DIR}/texture/TextureCache.h
    ${REAPER_RENDERER_DIR}/texture/TextureLoader.cpp
    ${REAPER_RENDERER_DIR}/texture/TextureLoader.h

    ${REAPER_RENDERER_DIR}/vulkan/Memory.cpp
    ${REAPER_RENDERER_DIR}/vulkan/Memory.h
    ${REAPER_RENDERER_DIR}/vulkan/PresentationSurface.cpp
    ${REAPER_RENDERER_DIR}/vulkan/PresentationSurface.h
    ${REAPER_RENDERER_DIR}/vulkan/SwapchainRendererBase.cpp
    ${REAPER_RENDERER_DIR}/vulkan/SwapchainRendererBase.h
    ${REAPER_RENDERER_DIR}/vulkan/Shader.cpp
    ${REAPER_RENDERER_DIR}/vulkan/Shader.h
    ${REAPER_RENDERER_DIR}/vulkan/VulkanRenderer.cpp
    ${REAPER_RENDERER_DIR}/vulkan/VulkanRenderer.h

    ${REAPER_RENDERER_DIR}/vulkan/allocator/GPUStackAllocator.cpp
    ${REAPER_RENDERER_DIR}/vulkan/allocator/GPUStackAllocator.h

    ${REAPER_RENDERER_DIR}/vulkan/api/Vulkan.h
    ${REAPER_RENDERER_DIR}/vulkan/api/VulkanFunctions.cpp
    ${REAPER_RENDERER_DIR}/vulkan/api/VulkanFunctions.h
    ${REAPER_RENDERER_DIR}/vulkan/api/VulkanSymbolHelper.inl
)

set(REAPER_RENDERER_TEST_SRCS
    ${REAPER_RENDERER_DIR}/test/mesh_loading.cpp
    ${REAPER_RENDERER_DIR}/test/renderer.cpp
    ${REAPER_RENDERER_DIR}/test/scene.cpp
    ${REAPER_RENDERER_DIR}/test/culling.cpp
    ${REAPER_RENDERER_DIR}/test/test_main.cpp
    ${REAPER_RENDERER_DIR}/test/texture_loading.cpp
    ${REAPER_RENDERER_DIR}/test/window.cpp
)

set(REAPER_GLSL_SHADERS
    ${CMAKE_SOURCE_DIR}/res/shader/samples/tri.frag
    ${CMAKE_SOURCE_DIR}/res/shader/samples/tri.vert
    ${CMAKE_SOURCE_DIR}/res/shader/samples/push_constants.frag
    ${CMAKE_SOURCE_DIR}/res/shader/samples/push_constants.vert
    ${CMAKE_SOURCE_DIR}/res/shader/samples/texture.frag
    ${CMAKE_SOURCE_DIR}/res/shader/samples/texture.vert
    ${CMAKE_SOURCE_DIR}/res/shader/samples/uniform.frag
    ${CMAKE_SOURCE_DIR}/res/shader/samples/uniform.vert
    ${CMAKE_SOURCE_DIR}/res/shader/mesh_shade.frag
    ${CMAKE_SOURCE_DIR}/res/shader/mesh_shade.vert
)

add_glslang_spirv_targets(REAPER_SPIRV_OUTPUT ${REAPER_GLSL_SHADERS})

add_library(${REAPER_RENDERER_BIN} ${REAPER_BUILD_TYPE} ${REAPER_RENDERER_SRCS} ${REAPER_GLSL_SHADERS} ${REAPER_SPIRV_OUTPUT})
reaper_add_custom_build_flags(${REAPER_RENDERER_BIN} "Renderer")

target_include_directories(${REAPER_RENDERER_BIN} PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${REAPER_RENDERER_BIN} SYSTEM PUBLIC ${REAPER_RENDERER_INCLUDES})

#Libraries
target_link_libraries(${REAPER_RENDERER_BIN} ${REAPER_RENDERER_LIBRARIES})

set_target_properties(${REAPER_RENDERER_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_RENDERER_BIN})

if (REAPER_BUILD_TESTS)
    reaper_add_tests(${REAPER_RENDERER_BIN} ${REAPER_RENDERER_TEST_SRCS})
endif()
