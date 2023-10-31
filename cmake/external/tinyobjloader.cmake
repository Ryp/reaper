#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2022 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(TINYOBJLOADER_BIN tinyobjloader)

add_library(${TINYOBJLOADER_BIN} STATIC)

set(TINYOBJLOADER_PATH ${CMAKE_SOURCE_DIR}/external/tinyobjloader)

target_sources(${TINYOBJLOADER_BIN} PRIVATE
    ${TINYOBJLOADER_PATH}/tiny_obj_loader.cc
    ${TINYOBJLOADER_PATH}/tiny_obj_loader.h
)

target_include_directories(${TINYOBJLOADER_BIN} SYSTEM INTERFACE ${TINYOBJLOADER_PATH})

reaper_configure_external_target(${TINYOBJLOADER_BIN} "TinyObjLoader" ${TINYOBJLOADER_PATH})

set_target_properties(${TINYOBJLOADER_BIN} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${TINYOBJLOADER_BIN} PROPERTIES FOLDER External)
