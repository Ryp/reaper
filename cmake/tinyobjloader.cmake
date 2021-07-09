#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(TINYOBJLOADER_BIN tinyobjloader)

add_library(${TINYOBJLOADER_BIN} STATIC)

target_sources(${TINYOBJLOADER_BIN} PRIVATE
    ${CMAKE_SOURCE_DIR}/external/tinyobjloader/tiny_obj_loader.cc
    ${CMAKE_SOURCE_DIR}/external/tinyobjloader/tiny_obj_loader.h
)

target_include_directories(${TINYOBJLOADER_BIN} SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/external/tinyobjloader)

reaper_configure_external_target(${TINYOBJLOADER_BIN} "TinyObjLoader")

set_target_properties(${TINYOBJLOADER_BIN} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${TINYOBJLOADER_BIN} PROPERTIES FOLDER External)
