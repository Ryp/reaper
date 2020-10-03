#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2020 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

if(MSVC)
    # Visual Studio 2017 or newer
    if(MSVC_VERSION LESS 1910)
        message(FATAL_ERROR "This version of Visual Studio is not supported")
    elseif(MSVC_VERSION GREATER 1920)
        message(WARNING "This version of Visual Studio is not officially supported yet. Proceed at your own risk.")
    endif()
endif()
