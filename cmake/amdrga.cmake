# Find the amd rga spirv compiler library
#
# FIXME
set(AMD_RGA_COMMON_PATH ${CMAKE_SOURCE_DIR}/external/amd-rga/Core/VulkanOffline)
if(UNIX)
    set(AMD_RGA_PATH ${AMD_RGA_COMMON_PATH}/lnx64)
elseif(WIN32)
    set(AMD_RGA_PATH ${AMD_RGA_COMMON_PATH}/win64)
endif()

find_program(AMD_RGA_EXEC amdspv HINT ${AMD_RGA_PATH})

if(NOT AMD_RGA_EXEC)
    message(FATAL_ERROR "${AMD_RGA_EXEC} not found")
endif()

# Generate targets for GCN ISA info building
# Set generated_files to the name of the output variable you want to retrieve (and later depend on)
# Pass a list of SPIR-V files to generate GCN ISA for in the rest of the arguments.
function(add_spirv_to_gcn_targets generated_files)
    # On unix we need to manually add spvgen.so to the library path
    if(UNIX)
        get_filename_component(AMD_RGA_PATH ${AMD_RGA_EXEC} DIRECTORY)
        set(UNIX_SPECIFIC "LD_LIBRARY_PATH=${AMD_RGA_PATH}")
    endif()

    set(OUTPUT_GCN_FILES)
    foreach(INPUT_SPIRV IN LISTS ARGN)
        get_filename_component(INPUT_BASE ${INPUT_SPIRV} NAME_WLE)

        set(OUTPUT_GCN_ISA_FILE "${INPUT_BASE}.gcn")
        set(OUTPUT_GCN_STATS_FILE "${INPUT_BASE}.gcn-stats")
        set(OUTPUT_GCN_VERSION -gfxip 8)

        add_custom_command(OUTPUT ${OUTPUT_GCN_ISA_FILE} ${OUTPUT_GCN_STATS_FILE}
            COMMAND ${UNIX_SPECIFIC} ${AMD_RGA_EXEC}
                ${OUTPUT_GCN_VERSION} -set defaultOutput=0 in.spv=${INPUT_SPIRV}
                out.vert.isaText=${OUTPUT_GCN_ISA_FILE} out.vert.isaInfo=${OUTPUT_GCN_STATS_FILE}
                out.geom.isaText=${OUTPUT_GCN_ISA_FILE} out.geom.isaInfo=${OUTPUT_GCN_STATS_FILE}
                out.tesc.isaText=${OUTPUT_GCN_ISA_FILE} out.tesc.isaInfo=${OUTPUT_GCN_STATS_FILE}
                out.tese.isaText=${OUTPUT_GCN_ISA_FILE} out.tese.isaInfo=${OUTPUT_GCN_STATS_FILE}
                out.frag.isaText=${OUTPUT_GCN_ISA_FILE} out.frag.isaInfo=${OUTPUT_GCN_STATS_FILE}
                out.comp.isaText=${OUTPUT_GCN_ISA_FILE} out.comp.isaInfo=${OUTPUT_GCN_STATS_FILE}
            MAIN_DEPENDENCY ${INPUT_SPIRV}
            COMMENT "Disassembling SPIR-V program ${INPUT_SPIRV} into GCN ISA"
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            VERBATIM
        )
        list(APPEND OUTPUT_GCN_FILES ${OUTPUT_GCN_ISA_FILE} ${OUTPUT_GCN_STATS_FILE})
    endforeach()
    set(${generated_files} "${OUTPUT_GCN_FILES}" PARENT_SCOPE)
endfunction()
