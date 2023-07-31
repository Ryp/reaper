#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2023 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_profiling)

add_library(${target} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Scope.h)

if(REAPER_USE_TRACY)
    include(tracy)
    target_include_directories(${target} SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/tracy/public)
    target_compile_definitions(${target} INTERFACE REAPER_USE_TRACY)
    target_link_libraries(${target} INTERFACE Tracy::TracyClient reaper_vulkan_loader)
endif()