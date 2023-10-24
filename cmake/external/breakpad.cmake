#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2023 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(GOOGLE_BREAKPAD_PATH ${CMAKE_SOURCE_DIR}/external/google/breakpad/src)

add_library(google_breakpad INTERFACE)

target_include_directories(google_breakpad SYSTEM INTERFACE ${GOOGLE_BREAKPAD_PATH}/src)

if(WIN32)
    target_include_directories(google_breakpad SYSTEM INTERFACE ${GOOGLE_BREAKPAD_PATH}/src/client/windows)
    target_link_directories(google_breakpad INTERFACE ${GOOGLE_BREAKPAD_PATH}/src/client/windows)
elseif(UNIX)
    target_include_directories(google_breakpad SYSTEM INTERFACE ${GOOGLE_BREAKPAD_PATH}/src/client/linux)
    target_link_directories(google_breakpad INTERFACE ${GOOGLE_BREAKPAD_PATH}/src/client/linux)
endif()

target_link_libraries(google_breakpad INTERFACE breakpad_client)
