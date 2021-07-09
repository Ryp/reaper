#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

find_program(CLANG_FORMAT clang-format)

if(NOT CLANG_FORMAT)
    message(STATUS "Could not find clang-format")
else()
    # Get the output from --version
    set(CLANG_FORMAT_VERSION_CMD ${CLANG_FORMAT} --version)
    execute_process(COMMAND ${CLANG_FORMAT_VERSION_CMD} OUTPUT_VARIABLE CLANG_FORMAT_VERSION_FULL_STRING)

    # Parse it and extract major, minor and patch
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CLANG_FORMAT_VERSION_MATCH ${CLANG_FORMAT_VERSION_FULL_STRING})
    set_program_version(CLANG_FORMAT ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3})

    unset(CLANG_FORMAT_VERSION_CMD)
    unset(CLANG_FORMAT_VERSION_FULL_STRING)
    unset(CLANG_FORMAT_VERSION_MATCH)

    message(STATUS "Found clang-format: ${CLANG_FORMAT_VERSION}")
endif()
