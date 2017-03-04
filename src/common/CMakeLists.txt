#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(REAPER_COMMON_LIBRARIES ${REAPER_COMMON_LIBRARIES} ${REAPER_CORE_BIN})

set(REAPER_COMMON_BIN reaper_common)
set(REAPER_COMMON_BIN ${REAPER_COMMON_BIN} PARENT_SCOPE)

set(REAPER_COMMON_DIR ${CMAKE_SOURCE_DIR}/src/common)
set(REAPER_COMMON_SRCS
    ${REAPER_COMMON_DIR}/DebugLog.cpp
    ${REAPER_COMMON_DIR}/DebugLog.h
    ${REAPER_COMMON_DIR}/Log.cpp
    ${REAPER_COMMON_DIR}/Log.h

    ${REAPER_COMMON_DIR}/ReaperRoot.h
)

set(REAPER_COMMON_TEST_SRCS
    ${REAPER_COMMON_DIR}/test/log.cpp
    ${REAPER_COMMON_DIR}/test/test_main.cpp
)

add_library(${REAPER_COMMON_BIN} ${REAPER_BUILD_TYPE} ${REAPER_COMMON_SRCS})
reaper_add_custom_build_flags(${REAPER_COMMON_BIN} "Common")

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_COMMON_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_COMMON_BIN} SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/external/fmt
    ${REAPER_COMMON_INCLUDES})

#Libraries
target_link_libraries(${REAPER_COMMON_BIN} ${REAPER_COMMON_LIBRARIES})

set_target_properties(${REAPER_COMMON_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_COMMON_BIN})

if (REAPER_BUILD_TESTS)
    reaper_add_tests(${REAPER_COMMON_BIN} ${REAPER_COMMON_TEST_SRCS})
endif()
