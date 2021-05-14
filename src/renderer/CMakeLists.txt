#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2020 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_renderer)
set(REAPER_RENDERER_BIN ${target} PARENT_SCOPE)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/PrepareBuckets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PrepareBuckets.h

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

    ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/Types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hlsl/Types.inl

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
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/VulkanRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/VulkanRenderer.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/AMDVulkanMemoryAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/AMDVulkanMemoryAllocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/GPUStackAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/GPUStackAllocator.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/Vulkan.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanFunctions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanFunctions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanStringConversion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanStringConversion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanSymbolHelper.inl

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/Culling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/Culling.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/MainPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/MainPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/ShadowMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/ShadowMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/SwapchainPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/SwapchainPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TestGraphics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TestGraphics.h

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
    ${REAPER_SHADER_DIR}/fullscreen_triangle.vert.hlsl
    ${REAPER_SHADER_DIR}/matrix_row_major_codegen_bug.comp.hlsl
    ${REAPER_SHADER_DIR}/mesh_shade.frag.glsl
    ${REAPER_SHADER_DIR}/mesh_shade.vert.glsl
    ${REAPER_SHADER_DIR}/mesh_simple.frag.glsl
    ${REAPER_SHADER_DIR}/mesh_simple.vert.glsl
    ${REAPER_SHADER_DIR}/mesh_transformed.frag.glsl
    ${REAPER_SHADER_DIR}/mesh_transformed_shaded.frag.hlsl
    ${REAPER_SHADER_DIR}/mesh_transformed_shaded.vert.hlsl
    ${REAPER_SHADER_DIR}/mesh_transformed.vert.glsl
    ${REAPER_SHADER_DIR}/render_shadow.frag.hlsl
    ${REAPER_SHADER_DIR}/render_shadow.vert.hlsl
    ${REAPER_SHADER_DIR}/swapchain_write.frag.hlsl
    ${REAPER_SHADER_DIR}/test.comp.glsl
)

# Shader compilation
include(glslang)
include(amdrga)

if(REAPER_HLSL_USE_DXC)
    include(dxc)
endif()

# FIXME share this between CMake and the corresponding C++ code
# NOTE: bad codegen with glslang when targeting vulkan1.2
# https://github.com/KhronosGroup/glslang/issues/2411
set(REAPER_VULKAN_VERSION vulkan1.1)
set(REAPER_SHADER_ENTRY_POINT main)

# Loop variables
set(REAPER_SPIRV_OUTPUT)
set(REAPER_GCN_OUTPUT)

# Process each shader
foreach(INPUT_SHADER IN LISTS REAPER_SHADER_SRCS)
    # Build the correct output name and path
    file(RELATIVE_PATH INPUT_SHADER_REL ${REAPER_SHADER_DIR} ${INPUT_SHADER})
    get_filename_component(INPUT_SHADER_REL_WLE ${INPUT_SHADER_REL} NAME_WLE)

    set(OUTPUT_SPIRV "${CMAKE_BINARY_DIR}/shader/${INPUT_SHADER_REL_WLE}.spv")

    get_filename_component(OUTPUT_SPIRV_PATH ${OUTPUT_SPIRV} DIRECTORY)
    get_filename_component(OUTPUT_SPIRV_NAME ${OUTPUT_SPIRV} NAME)

    get_filename_component(INPUT_SHADER_EXTENSION ${INPUT_SHADER} LAST_EXT)

    if(INPUT_SHADER_EXTENSION STREQUAL ".hlsl")
        # Watch GDC 2018 - HLSL in Vulkan
        # https://www.youtube.com/watch?v=42lqJ-iXc7g
        if(REAPER_HLSL_USE_DXC)
            set(REAPER_DXC_SM_MAJOR 6)
            set(REAPER_DXC_SM_MINOR 0)
            get_filename_component(SHADER_TYPE_EXTENSION ${INPUT_SHADER} NAME_WLE)
            get_filename_component(SHADER_TYPE_EXTENSION ${SHADER_TYPE_EXTENSION} LAST_EXT)
            dxc_get_shader_model_string(${SHADER_TYPE_EXTENSION} ${REAPER_DXC_SM_MAJOR} ${REAPER_DXC_SM_MINOR} SHADER_MODEL_STRING)

            add_custom_command(OUTPUT ${OUTPUT_SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_SPIRV_PATH}
                COMMAND ${DXC_EXEC} -nologo -T ${SHADER_MODEL_STRING} -spirv -fspv-target-env=${REAPER_VULKAN_VERSION} -D_DXC -Zi -E ${REAPER_SHADER_ENTRY_POINT} ${INPUT_SHADER} -I${REAPER_SHADER_DIR} -Fo ${OUTPUT_SPIRV}.unoptimized
                COMMAND ${VULKAN_SPIRV_OPT_EXEC} ${OUTPUT_SPIRV}.unoptimized --legalize-hlsl -Os -o ${OUTPUT_SPIRV}
                COMMAND ${VULKAN_SPIRV_DIS_EXEC} ${OUTPUT_SPIRV} -o ${OUTPUT_SPIRV}asm
                COMMAND ${VULKAN_SPIRV_VAL_EXEC} ${OUTPUT_SPIRV}
                MAIN_DEPENDENCY ${INPUT_SHADER}
                COMMENT "Compiling HLSL shader ${INPUT_SHADER_REL} to SPIR-V (${OUTPUT_SPIRV_NAME})"
                VERBATIM)
        else()
            add_custom_command(OUTPUT ${OUTPUT_SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_SPIRV_PATH}
                # https://github.com/KhronosGroup/glslang/issues/1461
                COMMAND ${VULKAN_GLSLANGVALIDATOR_EXEC} --target-env ${REAPER_VULKAN_VERSION} -g -e ${REAPER_SHADER_ENTRY_POINT} -V -D ${INPUT_SHADER} -I${REAPER_SHADER_DIR} -o ${OUTPUT_SPIRV}.unoptimized
                COMMAND ${VULKAN_SPIRV_OPT_EXEC} ${OUTPUT_SPIRV}.unoptimized --legalize-hlsl -Os -o ${OUTPUT_SPIRV}
                COMMAND ${VULKAN_SPIRV_DIS_EXEC} ${OUTPUT_SPIRV} -o ${OUTPUT_SPIRV}asm
                COMMAND ${VULKAN_SPIRV_VAL_EXEC} ${OUTPUT_SPIRV}
                MAIN_DEPENDENCY ${INPUT_SHADER}
                COMMENT "Compiling HLSL shader ${INPUT_SHADER_REL} to SPIR-V (${OUTPUT_SPIRV_NAME})"
                VERBATIM)
        endif()

        generate_gcn_output(${OUTPUT_SPIRV} REAPER_GCN_OUTPUT)
    elseif(INPUT_SHADER_EXTENSION STREQUAL ".glsl")
        add_custom_command(OUTPUT ${OUTPUT_SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_SPIRV_PATH}
            COMMAND ${VULKAN_GLSLANGVALIDATOR_EXEC} --target-env ${REAPER_VULKAN_VERSION} -g -V ${INPUT_SHADER} -I${REAPER_SHADER_DIR} -o ${OUTPUT_SPIRV}.unoptimized
            COMMAND ${VULKAN_SPIRV_OPT_EXEC} ${OUTPUT_SPIRV}.unoptimized -Os -o ${OUTPUT_SPIRV}
            COMMAND ${VULKAN_SPIRV_DIS_EXEC} ${OUTPUT_SPIRV} -o ${OUTPUT_SPIRV}asm
            COMMAND ${VULKAN_SPIRV_VAL_EXEC} ${OUTPUT_SPIRV}
            MAIN_DEPENDENCY ${INPUT_SHADER}
            COMMENT "Compiling GLSL shader ${INPUT_SHADER_REL} to SPIR-V (${OUTPUT_SPIRV_NAME})"
            VERBATIM)

        generate_gcn_output(${OUTPUT_SPIRV} REAPER_GCN_OUTPUT)
    else()
        message(FATAL_ERROR "Unknown shader type for ${INPUT_SHADER_REL}")
    endif()

    list(APPEND REAPER_SPIRV_OUTPUT ${OUTPUT_SPIRV})
endforeach()

target_sources(${target} PRIVATE
    ${REAPER_SHADER_SRCS}
    ${REAPER_SPIRV_OUTPUT}
    ${REAPER_GCN_OUTPUT})

find_package(Vulkan REQUIRED)
target_compile_definitions(${target} PRIVATE REAPER_VK_LIB_NAME="${VULKAN_LIBRARY}")

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
    amd-vma
    meshoptimizer
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
    ${CMAKE_CURRENT_SOURCE_DIR}/test/hlsl/float_vector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/hlsl/float_matrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/hlsl/struct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/texture_loading.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/window.cpp
)
