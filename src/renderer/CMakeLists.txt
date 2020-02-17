#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2019 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_renderer)
set(REAPER_RENDERER_BIN ${target} PARENT_SCOPE)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.h

    ${CMAKE_CURRENT_SOURCE_DIR}/Material.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Material.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RendererExport.h

    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.h

    ${CMAKE_CURRENT_SOURCE_DIR}/format/FloatTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/FloatTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/format/PixelFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/PixelFormat.h

    ${CMAKE_CURRENT_SOURCE_DIR}/graph/FrameGraphBasicTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/graph/FrameGraphBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graph/FrameGraphBuilder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/graph/FrameGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graph/FrameGraph.h
    ${CMAKE_CURRENT_SOURCE_DIR}/graph/GraphDebug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graph/GraphDebug.h

    ${CMAKE_CURRENT_SOURCE_DIR}/texture/GPUTextureProperties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/GPUTextureProperties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/Texture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/TextureCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/TextureCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/TextureLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/TextureLoader.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Culling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Culling.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Debug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/PresentationSurface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/PresentationSurface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Swapchain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Swapchain.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/SwapchainRendererBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/SwapchainRendererBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Test.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/TestGraphics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/TestGraphics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/VulkanRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/VulkanRenderer.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/GPUStackAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/GPUStackAllocator.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/Vulkan.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanFunctions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanFunctions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanStringConversion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanStringConversion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanSymbolHelper.inl

    ${CMAKE_CURRENT_SOURCE_DIR}/window/Event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Window.h

    ${CMAKE_CURRENT_SOURCE_DIR}/renderdoc/RenderDoc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderdoc/RenderDoc.h
)

if(REAPER_USE_RENDERDOC)
    find_package(RenderDoc REQUIRED)
    target_link_libraries(${target} PRIVATE RenderDoc::RenderDoc)
    target_compile_definitions(${target} PRIVATE REAPER_USE_RENDERDOC)
    target_compile_definitions(${target} PRIVATE REAPER_RENDERDOC_LIB_NAME="${RENDERDOC_LIBRARY}")
endif()

set(REAPER_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shader)
set(REAPER_SHADER_SRCS
    ${REAPER_SHADER_DIR}/samples/push_constants.frag.glsl
    ${REAPER_SHADER_DIR}/samples/push_constants.vert.glsl
    ${REAPER_SHADER_DIR}/samples/texture.frag.glsl
    ${REAPER_SHADER_DIR}/samples/texture.vert.glsl
    ${REAPER_SHADER_DIR}/samples/tri.frag.glsl
    ${REAPER_SHADER_DIR}/samples/tri.vert.glsl
    ${REAPER_SHADER_DIR}/samples/uniform.frag.glsl
    ${REAPER_SHADER_DIR}/samples/uniform.vert.glsl
    ${REAPER_SHADER_DIR}/blit.frag.glsl
    ${REAPER_SHADER_DIR}/blit.vert.glsl
    ${REAPER_SHADER_DIR}/compaction_prepare_indirect_dispatch.comp.hlsl
    ${REAPER_SHADER_DIR}/cull_triangle_batch.comp.hlsl
    ${REAPER_SHADER_DIR}/draw_command_compaction.comp.hlsl
    ${REAPER_SHADER_DIR}/mesh_shade.frag.glsl
    ${REAPER_SHADER_DIR}/mesh_shade.vert.glsl
    ${REAPER_SHADER_DIR}/mesh_simple.frag.glsl
    ${REAPER_SHADER_DIR}/mesh_simple.vert.glsl
    ${REAPER_SHADER_DIR}/mesh_transformed.frag.glsl
    ${REAPER_SHADER_DIR}/mesh_transformed_shaded.frag.hlsl
    ${REAPER_SHADER_DIR}/mesh_transformed_shaded.vert.hlsl
    ${REAPER_SHADER_DIR}/mesh_transformed.vert.glsl
    ${REAPER_SHADER_DIR}/test.comp.glsl
)

# Shader compilation
include(glslang)

add_glslang_spirv_targets(${REAPER_SHADER_DIR} REAPER_SPIRV_OUTPUT ${REAPER_SHADER_SRCS})
target_sources(${target} PRIVATE ${REAPER_SHADER_SRCS} ${REAPER_SPIRV_OUTPUT})

include(amdrga)
add_spirv_to_gcn_targets(REAPER_GCN_OUTPUT ${REAPER_SPIRV_OUTPUT})
target_sources(${target} PRIVATE ${REAPER_GCN_OUTPUT})

# Vulkan dependency
find_package(Vulkan REQUIRED)

# Choose windowing system
if(WIN32)
    target_compile_definitions(${target} PUBLIC VK_USE_PLATFORM_WIN32_KHR)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/window/Win32Window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/window/Win32Window.h)
elseif(APPLE OR UNIX)
    if(ON) # Prefer XCB
        find_package(XCB REQUIRED)
        target_link_libraries(${target} PUBLIC ${LIBXCB_LIBRARIES})
        target_include_directories(${target} SYSTEM PUBLIC ${LIBXCB_INCLUDE_DIR})
        target_compile_definitions(${target} PUBLIC VK_USE_PLATFORM_XCB_KHR)
        target_sources(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/window/XCBWindow.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/window/XCBWindow.h)
    else()
        find_package(X11 REQUIRED)
        target_link_libraries(${target} PUBLIC ${X11_LIBRARIES})
        target_include_directories(${target} SYSTEM PUBLIC ${X11_INCLUDE_DIR})
        target_compile_definitions(${target} PUBLIC VK_USE_PLATFORM_XLIB_KHR)
        target_sources(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/window/XLibWindow.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/window/XLibWindow.h)
    endif()
else()
    # We could allow headless instances without problems though
    message(FATAL_ERROR "Could not detect platform!")
endif()

target_link_libraries(${target} PUBLIC
    ${REAPER_CORE_BIN}
    ${REAPER_MESH_BIN}
    ${REAPER_COMMON_BIN}
    ${REAPER_INPUT_BIN}
    Vulkan::Vulkan
    fmt
    gli
    glm
    span
)

reaper_configure_library(${target} "Renderer")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

reaper_add_tests(${target}
    #${CMAKE_CURRENT_SOURCE_DIR}/test/culling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/graph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mesh_loading.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/texture_loading.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/window.cpp
)
