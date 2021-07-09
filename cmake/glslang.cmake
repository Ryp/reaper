#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
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
else()
    # Get the output from --version
    set(GLSLANGVALIDATOR_VERSION_CMD ${VULKAN_GLSLANGVALIDATOR_EXEC} --version)
    execute_process(COMMAND ${GLSLANGVALIDATOR_VERSION_CMD} OUTPUT_VARIABLE GLSLANGVALIDATOR_VERSION_FULL_STRING)

    # Parse it and extract major, minor and patch
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" GLSLANG_VERSION_MATCH ${GLSLANGVALIDATOR_VERSION_FULL_STRING})
    set_program_version(GLSLANG ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3})

    unset(GLSLANG_VERSION_CMD)
    unset(GLSLANG_VERSION_MATCH)
    unset(GLSLANGVALIDATOR_VERSION_FULL_STRING)

    message(STATUS "Found glslang: ${GLSLANG_VERSION}")
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
