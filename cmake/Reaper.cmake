#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2018 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

include(${CMAKE_SOURCE_DIR}/cmake/Platform.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/Compiler.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/clang-tidy.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/external.cmake)

# Ignore level-4 warning C4201: nonstandard extension used : nameless struct/union
# Ignore level-1 warning C4251: 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
set(REAPER_MSVC_DISABLE_WARNINGS "/W0")
set(REAPER_MSVC_DEBUG_FLAGS "/W4" "/wd4201" "/wd4251")
set(REAPER_MSVC_RELEASE_FLAGS ${REAPER_MSVC_DISABLE_WARNINGS})
set(REAPER_GCC_DEBUG_FLAGS
    "-Wall" "-Wextra" "-Wundef" "-Wshadow" "-funsigned-char"
    "-Wchar-subscripts" "-Wcast-align" "-Wwrite-strings" "-Wunused" "-Wuninitialized"
    "-Wpointer-arith" "-Wredundant-decls" "-Winline" "-Wformat"
    "-Wformat-security" "-Winit-self" "-Wdeprecated-declarations"
    "-Wmissing-include-dirs" "-Wmissing-declarations")
set(REAPER_GCC_RELEASE_FLAGS "")

# Use the same flags as GCC
set(REAPER_CLANG_DEBUG_FLAGS ${REAPER_GCC_DEBUG_FLAGS})
set(REAPER_CLANG_RELEASE_FLAGS ${REAPER_GCC_RELEASE_FLAGS})

# Useful for CMake-generated files
set(REAPER_GENERATED_BY_CMAKE_MSG "Generated by CMake")

# Sort sources in folders for visual studio
function(reaper_fill_vs_source_tree target root)
    get_target_property(SOURCE_FILES ${target} SOURCES)
    get_target_property(SOURCE_FILES_DEPS ${target} INTERFACE_SOURCES)
    list(APPEND SOURCE_FILES ${SOURCE_FILES_DEPS})

    foreach(FILE ${SOURCE_FILES})
        if (${FILE} MATCHES ".*\\.cpp" OR ${FILE} MATCHES ".*\\.inl" OR ${FILE} MATCHES ".*\\.h" OR ${FILE} MATCHES ".*\\.hpp")
            source_group(TREE ${CMAKE_SOURCE_DIR} PREFIX "Source Files" FILES ${FILE})
        elseif (${FILE} MATCHES ".*\\.natvis")
            source_group(TREE ${CMAKE_SOURCE_DIR} PREFIX "Natvis" FILES ${FILE})
        else()
            source_group(TREE ${CMAKE_SOURCE_DIR} PREFIX "Other" FILES ${FILE})
        endif()
    endforeach()
endfunction()

# Automatically generate the header used for exporting/importing symbols
function(reaper_generate_export_header target project_label)
    # Construct export macro name
    string(TOUPPER ${target} TARGET_UPPERCASE)
    set(LIBRARY_API_MACRO ${TARGET_UPPERCASE}_API)

    # Construct the macro that is used in to tell if the library is being built
    # or just imported. The naming convention is CMake specific.
    set(LIBRARY_BUILDING_MACRO ${target}_EXPORTS)

    # Generate the file from the template.
    set(REAPER_EXPORT_TEMPLATE_PATH ${CMAKE_SOURCE_DIR}/src/LibraryExport.h.in)
    set(LIBRARY_GENERATED_EXPORT_HEADER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${project_label}Export.h)
    configure_file(${REAPER_EXPORT_TEMPLATE_PATH} ${LIBRARY_GENERATED_EXPORT_HEADER_PATH} NEWLINE_STYLE UNIX @ONLY)
endfunction()

# Helper function that add default compilation flags for reaper targets
function(reaper_configure_warnings target enabled)
    if(${enabled})
        if(MSVC)
            target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_MSVC_DEBUG_FLAGS}>")
            target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${REAPER_MSVC_RELEASE_FLAGS}>")
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_GCC_DEBUG_FLAGS}>")
            target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${REAPER_GCC_RELEASE_FLAGS}>")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_CLANG_DEBUG_FLAGS}>")
            target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${REAPER_CLANG_RELEASE_FLAGS}>")
        else()
            message(FATAL_ERROR "Could not detect compiler")
        endif()
    else()
        if(MSVC)
            target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_MSVC_DISABLE_WARNINGS}>")
            target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${REAPER_MSVC_DISABLE_WARNINGS}>")
        endif()
    endif()
endfunction()

# Common helper that sets relevant C++ warnings and compilation flags
# see below for specific versions of the function.
# NOTE: call this function AFTER target_sources() calls
function(reaper_configure_target_common target project_label)
    target_include_directories(${target} PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(${target} PRIVATE REAPER_BUILD_${REAPER_BUILD_TYPE})
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
        set_target_properties(${target} PROPERTIES PROJECT_LABEL ${project_label})
        reaper_fill_vs_source_tree(${target} ${CMAKE_CURRENT_SOURCE_DIR})
        # /MP:              enable multi-threaded compilation
        # /std:c++latest:   added for nested namespaces (vs2015)
        # NOTE: it is mutually exclusive with /std:c++14
        target_compile_options(${target} PRIVATE "/MP" "/std:c++latest")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_target_properties(${target} PROPERTIES CXX_STANDARD 14)
        target_compile_options(${target} PRIVATE "-fvisibility=hidden")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_target_properties(${target} PROPERTIES CXX_STANDARD 14)
        target_compile_options(${target} PRIVATE "-fvisibility=hidden")
        add_clang_tidy_flags(${target})
    else()
        message(FATAL_ERROR "Could not detect compiler")
    endif()
endfunction()

# Append coverage compilation flags for the specified target
function(reaper_configure_coverage target)
    if(REAPER_BUILD_COVERAGE_INFO)
        # Sanity checks
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(FATAL_ERROR "Coverage info is only useful for unoptimized builds")
        endif()

        if(MSVC)
            message(FATAL_ERROR "Coverage is not supported for msvc")
        elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Set link flags
            target_link_libraries(${target} PRIVATE --coverage)
            # Set compile flags
            target_compile_options(${target} PRIVATE
                "--coverage"                # Enable coverage info generation
                "-O0"                       # Disable optimizations
                "-fno-elide-constructors"   # Inhibit inlining as much as possible
            )
        endif()
    endif()
endfunction()

# Use this function for engine libraries
function(reaper_configure_library target project_label)
    reaper_configure_target_common(${target} ${project_label})
    reaper_configure_warnings(${target} ON)
    reaper_configure_coverage(${target})
    reaper_generate_export_header(${target} ${project_label})
endfunction()

# Use this function for executables
function(reaper_configure_executable target project_label)
    reaper_configure_target_common(${target} ${project_label})
    reaper_configure_coverage(${target})
    reaper_configure_warnings(${target} ON)
endfunction()

# Use this function for external dependencies
function(reaper_configure_external_target target project_label)
    reaper_configure_target_common(${target} ${project_label})
    reaper_configure_warnings(${target} OFF)
endfunction()

# Reaper standard test helper
function(reaper_add_tests library testfiles)
    if(REAPER_BUILD_TESTS)
        set(REAPER_TEST_FILES ${testfiles} ${ARGN}) # Expecting a list here
        set(REAPER_TEST_BIN ${library}_tests)
        set(REAPER_TEST_SRCS ${REAPER_TEST_FILES} ${CMAKE_SOURCE_DIR}/src/test/test_main.cpp)

        add_executable(${REAPER_TEST_BIN} ${REAPER_TEST_SRCS})

        reaper_configure_target_common(${REAPER_TEST_BIN} "${REAPER_TEST_BIN}")
        reaper_configure_warnings(${REAPER_TEST_BIN} ON)
        reaper_configure_coverage(${REAPER_TEST_BIN})

        target_link_libraries(${REAPER_TEST_BIN}
            PUBLIC ${library}
            PRIVATE doctest
        )

        # Register wih ctest
        add_test(NAME ${REAPER_TEST_BIN} COMMAND $<TARGET_FILE:${REAPER_TEST_BIN}> WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        set_target_properties(${REAPER_TEST_BIN} PROPERTIES FOLDER Test)
        set_target_properties(${REAPER_TEST_BIN} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()
endfunction()

# Install and update git hooks
function(reaper_setup_git_hooks)
    if(REAPER_GIT_HOOKS_AUTO_UPDATE)
        include(clang-format)

        # Check if the executable is available
        if(NOT CLANG_FORMAT)
            message(FATAL_ERROR "clang-format is required")
        endif()

        set(REAPER_CLANG_FORMAT_REQUIRED_VERSION 6.0.0)

        # Check that we meet the requirements
        if(CLANG_FORMAT_VERSION VERSION_LESS ${REAPER_CLANG_FORMAT_REQUIRED_VERSION})
            message(FATAL_ERROR "clang-format ${REAPER_CLANG_FORMAT_REQUIRED_VERSION} is required")
        endif()

        message(STATUS "Updating git hooks")
        set(GIT_HOOKS_DIR ${CMAKE_SOURCE_DIR}/.git/hooks)
        set(REAPER_HOOKS_DIR ${CMAKE_SOURCE_DIR}/tools/git/hooks)

        # Setup hooks
        configure_file(${REAPER_HOOKS_DIR}/pre-commit.sh ${GIT_HOOKS_DIR}/pre-commit @ONLY)
    endif()
endfunction()

# Generic utility to declare version variables in the current scope
# ex: set_program_version(MYTOOL 4.5.6)
# will define MYTOOL_VERSION_MAJOR to 4, MYTOOL_VERSION_MINOR to 5, etc...
function(set_program_version program_prefix version_major version_minor version_patch)
    set(${program_prefix}_VERSION_MAJOR ${version_major} PARENT_SCOPE)
    set(${program_prefix}_VERSION_MINOR ${version_minor} PARENT_SCOPE)
    set(${program_prefix}_VERSION_PATCH ${version_patch} PARENT_SCOPE)
    set(${program_prefix}_VERSION "${version_major}.${version_minor}.${version_patch}" PARENT_SCOPE)
endfunction()
