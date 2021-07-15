#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(target reaper_math)

add_library(${target} ${REAPER_BUILD_TYPE})

target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/BasicMath.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FloatComparison.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MathExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Spline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Spline.h
)

target_link_libraries(${target} PUBLIC
    reaper_core
    glm
)

reaper_configure_library(${target} "Math")

set_target_properties(${target} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${target})

reaper_add_tests(${target}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/spline.cpp
)
