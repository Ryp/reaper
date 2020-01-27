#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2019 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

find_program(VULKAN_GLSLANGVALIDATOR_EXEC glslangValidator HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VK_SDK_PATH}/Bin")

find_program(VULKAN_SPIRV_OPT_EXEC spirv-opt HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VK_SDK_PATH}/Bin")

find_program(VULKAN_SPIRV_DIS_EXEC spirv-dis HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VK_SDK_PATH}/Bin")

find_program(VULKAN_SPIRV_VAL_EXEC spirv-val HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VK_SDK_PATH}/Bin")

if(NOT VULKAN_GLSLANGVALIDATOR_EXEC)
    message(FATAL_ERROR "${VULKAN_GLSLANGVALIDATOR_EXEC} not found")
endif()

if(NOT VULKAN_SPIRV_OPT_EXEC)
    message(FATAL_ERROR "${VULKAN_SPIRV_OPT_EXEC} not found")
endif()

if(NOT VULKAN_SPIRV_DIS_EXEC)
    message(FATAL_ERROR "${VULKAN_SPIRV_DIS_EXEC} not found")
endif()

if(NOT VULKAN_SPIRV_VAL_EXEC)
    message(FATAL_ERROR "${VULKAN_SPIRV_VAL_EXEC} not found")
endif()

# This function will create SPIR-V targets for each input glsl file and append it
# to the user-provided 'generated_files' variable.
# Then make your desired target depend on them and it will automatically compile your shaders.
function(add_glslang_spirv_targets shader_root_folder generated_files)
    if(${ARGC} LESS_EQUAL 2)
        message(FATAL_ERROR "No shaders were passed to the function")
    endif()
    set(OUTPUT_SPIRV_FILES)
    foreach(INPUT_SHADER IN LISTS ARGN)
        # Build the correct output name and path
        file(RELATIVE_PATH INPUT_SHADER_REL ${shader_root_folder} ${INPUT_SHADER})
        get_filename_component(INPUT_SHADER_REL_WLE ${INPUT_SHADER_REL} NAME_WLE)

        set(OUTPUT_SPIRV "${CMAKE_BINARY_DIR}/spv/${INPUT_SHADER_REL_WLE}.spv")

        get_filename_component(OUTPUT_SPIRV_PATH ${OUTPUT_SPIRV} DIRECTORY)
        get_filename_component(OUTPUT_SPIRV_NAME ${OUTPUT_SPIRV} NAME)

        get_filename_component(INPUT_SHADER_EXTENSION ${INPUT_SHADER} LAST_EXT)

        if(INPUT_SHADER_EXTENSION STREQUAL ".hlsl")
            # Watch GDC 2018 - HLSL in Vulkan
            # https://www.youtube.com/watch?v=42lqJ-iXc7g
            add_custom_command(OUTPUT ${OUTPUT_SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_SPIRV_PATH}
                COMMAND ${VULKAN_GLSLANGVALIDATOR_EXEC} --target-env spirv1.3 -g -e main -V -D ${INPUT_SHADER} -o ${OUTPUT_SPIRV}.unoptimized
                COMMAND ${VULKAN_SPIRV_OPT_EXEC} ${OUTPUT_SPIRV}.unoptimized --legalize-hlsl -Os -o ${OUTPUT_SPIRV}
                COMMAND ${VULKAN_SPIRV_DIS_EXEC} ${OUTPUT_SPIRV} -o ${OUTPUT_SPIRV}.txt
                COMMAND ${VULKAN_SPIRV_VAL_EXEC} ${OUTPUT_SPIRV}
                MAIN_DEPENDENCY ${INPUT_SHADER}
                COMMENT "Compiling HLSL shader ${INPUT_SHADER_REL} to SPIR-V (${OUTPUT_SPIRV_NAME})"
                VERBATIM)
        elseif(INPUT_SHADER_EXTENSION STREQUAL ".glsl")
            add_custom_command(OUTPUT ${OUTPUT_SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_SPIRV_PATH}
                COMMAND ${VULKAN_GLSLANGVALIDATOR_EXEC} --target-env spirv1.3 -g -V ${INPUT_SHADER} -o ${OUTPUT_SPIRV}.unoptimized
                COMMAND ${VULKAN_SPIRV_OPT_EXEC} ${OUTPUT_SPIRV}.unoptimized -Os -o ${OUTPUT_SPIRV}
                COMMAND ${VULKAN_SPIRV_DIS_EXEC} ${OUTPUT_SPIRV} -o ${OUTPUT_SPIRV}.txt
                COMMAND ${VULKAN_SPIRV_VAL_EXEC} ${OUTPUT_SPIRV}
                MAIN_DEPENDENCY ${INPUT_SHADER}
                COMMENT "Compiling GLSL shader ${INPUT_SHADER_REL} to SPIR-V (${OUTPUT_SPIRV_NAME})"
                VERBATIM)
        else()
            message(FATAL_ERROR "Unknown shader type for ${INPUT_SHADER_REL}")
        endif()

        list(APPEND OUTPUT_SPIRV_FILES ${OUTPUT_SPIRV})
    endforeach()
    set(${generated_files} "${OUTPUT_SPIRV_FILES}" PARENT_SCOPE)
endfunction()
