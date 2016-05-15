find_program(GLSLANGVALIDATOR_EXEC glslangValidator)
if(NOT GLSLANGVALIDATOR_EXEC)
    message(FATAL_ERROR "${GLSLANGVALIDATOR_EXEC} not found")
endif()

function(add_glslang_spirv_targets generated_files)
    set(OUTPUT_SPIRV_FILES)
    foreach(INPUT_GLSL ${ARGN})
        # Build the correct output name and path
        file(RELATIVE_PATH INPUT_GLSL_REL "${CMAKE_SOURCE_DIR}/rc/shader" ${INPUT_GLSL})
        set(OUTPUT_SPIRV "${CMAKE_BINARY_DIR}/spv/${INPUT_GLSL_REL}.spv")
        get_filename_component(OUTPUT_SPIRV_PATH ${OUTPUT_SPIRV} PATH)
        get_filename_component(OUTPUT_SPIRV_NAME ${OUTPUT_SPIRV} NAME)

        add_custom_command(OUTPUT ${OUTPUT_SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_SPIRV_PATH}
            COMMAND ${GLSLANGVALIDATOR_EXEC} -s -V ${INPUT_GLSL} -o ${OUTPUT_SPIRV}
            DEPENDS ${INPUT_GLSL}
            COMMENT "Compiling GLSL shader ${INPUT_GLSL_REL} into SPIR-V (${OUTPUT_SPIRV_NAME})"
            VERBATIM
        )
        list(APPEND OUTPUT_SPIRV_FILES ${OUTPUT_SPIRV})
    endforeach()
    set(${generated_files} "${OUTPUT_SPIRV_FILES}" PARENT_SCOPE)
endfunction()
