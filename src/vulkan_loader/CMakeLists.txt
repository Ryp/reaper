#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_vulkan_loader)

add_library(${target} ${REAPER_BUILD_TYPE})

# Choose windowing system
if(WIN32)
    target_compile_definitions(${target} PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE OR UNIX)
    if(ON) # Prefer XCB
        target_compile_definitions(${target} PUBLIC VK_USE_PLATFORM_XCB_KHR)
    else()
        target_compile_definitions(${target} PUBLIC VK_USE_PLATFORM_XLIB_KHR)
    endif()
else()
    # We could allow headless instances without problems though
    message(FATAL_ERROR "Could not detect platform!")
endif()

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Functions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SymbolHelper.inl
)

find_package(Vulkan 1.3 REQUIRED)

# Get the shared library name for each platform
get_filename_component(REAPER_VK_LIB_NAME ${Vulkan_LIBRARY} NAME_WLE)
target_compile_definitions(${target} PUBLIC REAPER_VK_LIB_NAME="${REAPER_VK_LIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")

target_link_libraries(${target} PUBLIC
    Vulkan::Headers
    reaper_core
    fmt
)

reaper_configure_library(${target} "VulkanLoader")
