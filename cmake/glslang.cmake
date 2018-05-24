#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2018 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

find_program(VULKAN_GLSLANGVALIDATOR_EXEC glslangValidator
    HINT "$ENV{VULKAN_SDK}/Bin")

if(NOT VULKAN_GLSLANGVALIDATOR_EXEC)
    message(FATAL_ERROR "${VULKAN_GLSLANGVALIDATOR_EXEC} not found")
endif()

# This function will create SPIR-V targets for each input glsl file and append it
# to the user-provided 'generated_files' variable.
# Then make your desired target depend on them and it will automatically compile your shaders.
function(add_glslang_spirv_targets generated_files)
    set(OUTPUT_SPIRV_FILES)
    foreach(INPUT_GLSL ${ARGN})
        # Build the correct output name and path
        file(RELATIVE_PATH INPUT_GLSL_REL "${CMAKE_SOURCE_DIR}/res/shader" ${INPUT_GLSL})
        set(OUTPUT_SPIRV "${CMAKE_BINARY_DIR}/spv/${INPUT_GLSL_REL}.spv")
        get_filename_component(OUTPUT_SPIRV_PATH ${OUTPUT_SPIRV} DIRECTORY)
        get_filename_component(OUTPUT_SPIRV_NAME ${OUTPUT_SPIRV} NAME)

        add_custom_command(OUTPUT ${OUTPUT_SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_SPIRV_PATH}
            COMMAND ${VULKAN_GLSLANGVALIDATOR_EXEC} -V ${INPUT_GLSL} -o ${OUTPUT_SPIRV}
            MAIN_DEPENDENCY ${INPUT_GLSL}
            COMMENT "Compiling GLSL shader ${INPUT_GLSL_REL} into SPIR-V (${OUTPUT_SPIRV_NAME})"
            VERBATIM
        )
        list(APPEND OUTPUT_SPIRV_FILES ${OUTPUT_SPIRV})
    endforeach()
    set(${generated_files} "${OUTPUT_SPIRV_FILES}" PARENT_SCOPE)
endfunction()
