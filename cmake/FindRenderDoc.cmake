#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2018 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

# Find RenderDoc
#
# RENDERDOC_ROOT        - Use this on WIN32 to find your local installation
#
# RENDERDOC_INCLUDE_DIR
# RENDERDOC_LIBRARY
# RENDERDOC_FOUND

set(RENDERDOC_ROOT CACHE PATH "RenderDoc installation directory")

find_path(RENDERDOC_INCLUDE_DIR
    NAMES renderdoc.h renderdoc_app.h
    PATHS ${RENDERDOC_ROOT}
    DOC "Include path for RenderDoc's API")

# Temporarily override suffixes to allow correct DLL search on WIN32
set(RENDERDOC_SUFFIXES_SAVE ${CMAKE_FIND_LIBRARY_SUFFIXES})

if(WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll")
endif()

find_library(RENDERDOC_LIBRARY
    NAMES renderdoc
    PATHS ${RENDERDOC_ROOT}
    DOC "Path of RenderDoc's dynamic library")

# Restore library suffixes
set(CMAKE_FIND_LIBRARY_SUFFIXES ${RENDERDOC_SUFFIXES_SAVE})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RenderDoc DEFAULT_MSG RENDERDOC_LIBRARY RENDERDOC_INCLUDE_DIR)

mark_as_advanced(RENDERDOC_INCLUDE_DIR RENDERDOC_LIBRARY)

# NOTE: We are not linking directly against renderdoc, RENDERDOC_LIBRARY is provided to enable dynamic loading
if(RenderDoc_FOUND AND NOT TARGET RenderDoc::RenderDoc)
    add_library(RenderDoc::RenderDoc INTERFACE IMPORTED)
    set_target_properties(RenderDoc::RenderDoc PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${RENDERDOC_INCLUDE_DIR}"
    )
endif()
