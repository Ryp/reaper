#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

find_program(CLANG_TIDY_EXEC clang-tidy)

if(NOT CLANG_TIDY_EXEC)
    message(STATUS "Could not find clang-tidy")
elseif(NOT REAPER_RUN_CLANG_TIDY)
    message(STATUS "Found clang-tidy but disabled")
else()
    message(STATUS "Found clang-tidy")
endif()

# This function will add clang-tidy flags to the provided target
function(add_clang_tidy_flags target)
    if(REAPER_RUN_CLANG_TIDY AND CLANG_TIDY_EXEC)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXEC}")
    endif()
endfunction()

