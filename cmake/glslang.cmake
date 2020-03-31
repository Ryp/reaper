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
