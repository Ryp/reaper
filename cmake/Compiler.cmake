#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2018 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

if(MSVC)
    # Visual Studio 2015 or newer
    if(MSVC_VERSION LESS 1900)
        message(FATAL_ERROR "This version of Visual Studio is not supported")
    elseif(MSVC_VERSION GREATER 1900)
        message(WARNING "This version of Visual Studio is not yet supported")
    endif()
endif()
