#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

# https://cmake.org/cmake/help/latest/variable/MSVC_VERSION.html
if(MSVC)
    # Visual Studio 2017 or newer
    if(MSVC_VERSION LESS 1930)
        message(FATAL_ERROR "This version of Visual Studio is not supported")
    endif()
endif()
