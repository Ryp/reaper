#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_renderer)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ExecuteFrame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ExecuteFrame.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PrepareBuckets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PrepareBuckets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RendererExport.h

    ${CMAKE_CURRENT_SOURCE_DIR}/buffer/GPUBufferProperties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer/GPUBufferView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer/GPUBufferView.h

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
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/GPUTextureView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/GPUTextureView.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/BackendResources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/BackendResources.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Barrier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Barrier.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Debug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/DescriptorSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/DescriptorSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Display.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Display.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/FrameGraphResources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/FrameGraphResources.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/FrameSync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/FrameSync.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/MaterialResources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/MaterialResources.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/MeshCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/MeshCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/PresentationSurface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/PresentationSurface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/RenderPassHelpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/SamplerResources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/SamplerResources.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/ShaderModules.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/ShaderModules.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Swapchain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/Swapchain.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/AMDVulkanMemoryAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/AMDVulkanMemoryAllocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/GPUStackAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/allocator/GPUStackAllocator.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanStringConversion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/api/VulkanStringConversion.h

    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/Audio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/Audio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/Culling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/Culling.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/DebugGeometryRenderPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/DebugGeometryRenderPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/ForwardPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/ForwardPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/FrameGraphPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/FrameGraphPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/GBufferPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/GBufferPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/GuiPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/GuiPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/HistogramPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/HistogramPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/LightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/LightingPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/ShadowMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/ShadowMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/SwapchainPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/SwapchainPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TestGraphics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TestGraphics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TiledLightingCommon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TiledLightingCommon.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TiledLightingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TiledLightingPass.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TiledRasterPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/renderpass/TiledRasterPass.h

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
    ${REAPER_SHADER_DIR}/copy_to_depth.frag.hlsl
    ${REAPER_SHADER_DIR}/debug_geometry/build_cmds.comp.hlsl
    ${REAPER_SHADER_DIR}/debug_geometry/draw.frag.hlsl
    ${REAPER_SHADER_DIR}/debug_geometry/draw.vert.hlsl
    ${REAPER_SHADER_DIR}/forward.frag.hlsl
    ${REAPER_SHADER_DIR}/forward.vert.hlsl
    ${REAPER_SHADER_DIR}/fullscreen_triangle.vert.hlsl
    ${REAPER_SHADER_DIR}/gbuffer/gbuffer_write_opaque.frag.hlsl
    ${REAPER_SHADER_DIR}/gbuffer/gbuffer_write_opaque.vert.hlsl
    ${REAPER_SHADER_DIR}/gui_write.frag.hlsl
    ${REAPER_SHADER_DIR}/histogram/reduce_histogram.comp.hlsl
    ${REAPER_SHADER_DIR}/matrix_row_major_codegen_bug.comp.hlsl
    ${REAPER_SHADER_DIR}/meshlet/cull_meshlet.comp.hlsl
    ${REAPER_SHADER_DIR}/meshlet/cull_triangle_batch.comp.hlsl
    ${REAPER_SHADER_DIR}/meshlet/prepare_fine_culling_indirect.comp.hlsl
    ${REAPER_SHADER_DIR}/shadow/render_shadow.vert.hlsl
    ${REAPER_SHADER_DIR}/sound/oscillator.comp.hlsl
    ${REAPER_SHADER_DIR}/swapchain_write.frag.hlsl
    ${REAPER_SHADER_DIR}/tiled_lighting/classify_volume.comp.hlsl
    ${REAPER_SHADER_DIR}/tiled_lighting/rasterize_light_volume.frag.hlsl
    ${REAPER_SHADER_DIR}/tiled_lighting/rasterize_light_volume.vert.hlsl
    ${REAPER_SHADER_DIR}/tiled_lighting/tile_depth_downsample.comp.hlsl
    ${REAPER_SHADER_DIR}/tiled_lighting/tiled_lighting.comp.hlsl
    ${REAPER_SHADER_DIR}/tiled_lighting/tiled_lighting_debug.comp.hlsl
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
    get_filename_component(INPUT_SHADER_REL_DIR ${INPUT_SHADER_REL} DIRECTORY)
    get_filename_component(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shader/${INPUT_SHADER_REL_DIR}" ABSOLUTE)

    set(OUTPUT_SPIRV "${SHADER_BINARY_DIR}/${INPUT_SHADER_REL_WLE}.spv")

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
                COMMAND ${GLSLANGVALIDATOR_EXEC} --target-env ${REAPER_VULKAN_VERSION} -g -e ${REAPER_SHADER_ENTRY_POINT} -V -D ${INPUT_SHADER} -I${REAPER_SHADER_DIR} -o ${OUTPUT_SPIRV}.unoptimized
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
            COMMAND ${GLSLANGVALIDATOR_EXEC} --target-env ${REAPER_VULKAN_VERSION} -g -V ${INPUT_SHADER} -I${REAPER_SHADER_DIR} -o ${OUTPUT_SPIRV}.unoptimized
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

# Allow the C++ side to have the same include path as shaders, this is important for shared files.
target_include_directories(${target} PUBLIC ${REAPER_SHADER_DIR})

# Choose windowing system
if(WIN32)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/window/Win32Window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/window/Win32Window.h)
elseif(APPLE OR UNIX)
    if(ON) # Prefer XCB
        find_package(XCB COMPONENTS XKB REQUIRED)
        target_link_libraries(${target} PUBLIC ${LIBXCB_LIBRARIES})
        target_include_directories(${target} SYSTEM PUBLIC ${LIBXCB_INCLUDE_DIR})
        target_sources(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/window/XCBWindow.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/window/XCBWindow.h)
    else()
        find_package(X11 REQUIRED)
        target_link_libraries(${target} PUBLIC ${X11_LIBRARIES})
        target_include_directories(${target} SYSTEM PUBLIC ${X11_INCLUDE_DIR})
        target_sources(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/window/XLibWindow.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/window/XLibWindow.h)
    endif()
else()
    # We could allow headless instances without problems though
    message(FATAL_ERROR "Could not detect platform!")
endif()

set(MESHOPT_BUILD_SHARED_LIBS ${REAPER_BUILD_SHARED_LIBS})
add_subdirectory(${CMAKE_SOURCE_DIR}/external/meshoptimizer ${CMAKE_BINARY_DIR}/external/meshoptimizer)
set_target_properties(meshoptimizer PROPERTIES FOLDER External)

include(imgui)

target_link_libraries(${target} PUBLIC
    reaper_core
    reaper_mesh
    reaper_common
    reaper_vulkan_loader
    fmt
    glm
    span
    amd-vma
    meshoptimizer
    tinyddsloader
    imgui
)

reaper_configure_library(${target} "Renderer")

reaper_add_tests(${target}
    #${CMAKE_CURRENT_SOURCE_DIR}/test/culling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/graph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/mesh_loading.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/hlsl/float_vector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/hlsl/float_matrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/hlsl/struct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/window.cpp
)
