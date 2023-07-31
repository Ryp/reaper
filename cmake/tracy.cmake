#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2023 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMAND "" ON)
option(TRACY_NO_FRAME_IMAGE "" ON)
option(TRACY_STATIC "" OFF)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/tracy ${CMAKE_BINARY_DIR}/external/tracy)