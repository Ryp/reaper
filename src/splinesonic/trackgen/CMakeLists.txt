#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(REAPER_TRACKGEN_LIBRARIES ${REAPER_TRACKGEN_LIBRARIES}
    ${REAPER_CORE_BIN}
    ${REAPER_MATH_BIN}
)

set(REAPER_TRACKGEN_BIN reaper_trackgen)
set(REAPER_TRACKGEN_BIN ${REAPER_TRACKGEN_BIN} PARENT_SCOPE)

set(REAPER_TRACKGEN_DIR ${CMAKE_SOURCE_DIR}/src/splinesonic/trackgen)
set(REAPER_TRACKGEN_SRCS
    ${REAPER_TRACKGEN_DIR}/Track.cpp
    ${REAPER_TRACKGEN_DIR}/Track.h
    ${REAPER_TRACKGEN_DIR}/TrackDebug.cpp
    ${REAPER_TRACKGEN_DIR}/TrackDebug.h
    ${REAPER_TRACKGEN_DIR}/TrackGenExport.h
)

set(REAPER_TRACKGEN_TEST_SRCS
    ${REAPER_TRACKGEN_DIR}/test/gen_simple.cpp
)

add_library(${REAPER_TRACKGEN_BIN} ${REAPER_BUILD_TYPE} ${REAPER_TRACKGEN_SRCS})
reaper_add_custom_build_flags(${REAPER_TRACKGEN_BIN} "TrackGen")

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_TRACKGEN_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_TRACKGEN_BIN} SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/external/glm
    ${REAPER_TRACKGEN_INCLUDES})

#Libraries
target_link_libraries(${REAPER_TRACKGEN_BIN} ${REAPER_TRACKGEN_LIBRARIES})

set_target_properties(${REAPER_TRACKGEN_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_TRACKGEN_BIN})

if (REAPER_BUILD_TESTS)
    reaper_add_tests(${REAPER_TRACKGEN_BIN} ${REAPER_TRACKGEN_TEST_SRCS})
endif()

