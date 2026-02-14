#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_core)

add_library(${target} ${REAPER_LINKAGE_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Assert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Assert.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BitTricks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Cast.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Crash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Crash.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CoreExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Debugger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Debugger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DynamicLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DynamicLibrary.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EnumHelper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StackTrace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StackTrace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Version.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in

    ${CMAKE_CURRENT_SOURCE_DIR}/fs/FileLoading.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fs/FileLoading.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fs/Path.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fs/Path.h

    ${CMAKE_CURRENT_SOURCE_DIR}/memory/Allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/Allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/BuddyAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/BuddyAllocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/LinearAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/LinearAllocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/StackAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/StackAllocator.h
)

if(UNIX)
    # Setup libunwind
    find_package(Libunwind REQUIRED)
    target_link_libraries(${target} PRIVATE Libunwind::Libunwind)

    # Link with libdl
    target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS})
endif()

target_link_libraries(${target} PRIVATE fmt)

reaper_configure_library(${target} "Core")

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/alignment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/buddy_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/stack_allocator.cpp
)
