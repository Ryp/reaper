#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2017 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(REAPER_MATH_LIBRARIES ${REAPER_CORE_BIN})
set(REAPER_MATH_INCLUDES ${CMAKE_SOURCE_DIR}/external/glm)

set(REAPER_MATH_BIN reaper_math)
set(REAPER_MATH_BIN ${REAPER_MATH_BIN} PARENT_SCOPE)

set(REAPER_MATH_DIR ${CMAKE_SOURCE_DIR}/src/math)
set(REAPER_MATH_SRCS
    ${REAPER_MATH_DIR}/BasicMath.h
    ${REAPER_MATH_DIR}/Constants.h
    ${REAPER_MATH_DIR}/FloatComparison.h
    ${REAPER_MATH_DIR}/MathExport.h
    ${REAPER_MATH_DIR}/Spline.cpp
    ${REAPER_MATH_DIR}/Spline.h
)

set(REAPER_MATH_TEST_SRCS
    ${REAPER_MATH_DIR}/test/spline.cpp
)

add_library(${REAPER_MATH_BIN} ${REAPER_BUILD_TYPE} ${REAPER_MATH_SRCS})
reaper_configure_library(${REAPER_MATH_BIN} "Math")

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_MATH_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_MATH_BIN} SYSTEM PUBLIC
    ${REAPER_MATH_INCLUDES})

#Libraries
target_link_libraries(${REAPER_MATH_BIN} ${REAPER_MATH_LIBRARIES})

set_target_properties(${REAPER_MATH_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_MATH_BIN})

if (REAPER_BUILD_TESTS)
    reaper_add_tests(${REAPER_MATH_BIN} ${REAPER_MATH_TEST_SRCS})
endif()
