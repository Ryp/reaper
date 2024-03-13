#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2023 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

set(GOOGLE_PATH ${CMAKE_SOURCE_DIR}/external/google)

set(GOOGLE_BREAKPAD_PATH ${GOOGLE_PATH}/breakpad/src)
set(GOOGLE_CRASHPAD_PATH ${GOOGLE_PATH}/crashpad/crashpad)
set(GOOGLE_DEPOT_TOOLS_PATH ${GOOGLE_PATH}/depot_tools)

list(APPEND CMAKE_PROGRAM_PATH ${GOOGLE_DEPOT_TOOLS_PATH})
find_program(GOOGLE_DEPOT_TOOLS_GCLIENT gclient REQUIRED)
find_program(GOOGLE_DEPOT_TOOLS_GN gn REQUIRED)

# gclient sync will try to update the local clone of depot_tools
# creating this file will inhibit that behavior
file(TOUCH ${GOOGLE_DEPOT_TOOLS_PATH}/.disable_auto_update)

# NOTE: Partial dependency checking.
# It's not enough to mark the gclient file as the only byproduct,
# as there's many more files produced by this command.
# For the sake simplicity, I'm not going to bother making that step bulletproof.
set(GOOGLE_CRASHPAD_GCLIENT_ENTRIES_FILE ${GOOGLE_PATH}/crashpad/.gclient_entries)

add_custom_command(OUTPUT ${GOOGLE_CRASHPAD_GCLIENT_ENTRIES_FILE}
    COMMENT "Configure google crashpad repo"
    WORKING_DIRECTORY ${GOOGLE_PATH}/crashpad
    COMMAND ${GOOGLE_DEPOT_TOOLS_GCLIENT} sync --nohooks
    VERBATIM)

add_custom_target(crashpad_configure_repo DEPENDS ${GOOGLE_CRASHPAD_GCLIENT_ENTRIES_FILE})

set(GOOGLE_CRASHPAD_BINARY_DIR ${CMAKE_BINARY_DIR}/external/crashpad)

# Lists of the expected output libraries
# depot_tools used to contain the ninja binary but doesn't anymore, so we are
# using crashpad's version as a fallback.
# FIXME this file might only get available once gclient tools are called.
if(WIN32)
    set(GOOGLE_CRASHPAD_NINJA_EXE ${GOOGLE_CRASHPAD_PATH}/third_party/ninja/ninja.exe)
    list(APPEND GOOGLE_CRASHPAD_CLIENT_LIBRARIES
        ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/client/client.lib
        ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/client/common.lib
        ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/util/util.lib
        ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/third_party/mini_chromium/mini_chromium/base/base.lib)
elseif(UNIX)
    set(GOOGLE_CRASHPAD_NINJA_EXE ${GOOGLE_CRASHPAD_PATH}/third_party/ninja/ninja)
    list(APPEND GOOGLE_CRASHPAD_CLIENT_LIBRARIES
        ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/client/libclient.a
        ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/client/libcommon.a
        ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/util/libutil.a
        ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/third_party/mini_chromium/mini_chromium/base/libbase.a)
endif()

set(GN_IS_DEBUG)
set(GN_LINK_FLAG)

if(REAPER_DEBUG_BUILD)
    set(GN_IS_DEBUG true)
else()
    set(GN_IS_DEBUG false)
endif()

if(MSVC)
    if(REAPER_DEBUG_BUILD)
        set(GN_LINK_FLAG /MDd)
    else()
        set(GN_LINK_FLAG /MD)
    endif()
endif()

set(GOOGLE_CRASHPAD_NINJA_FILE ${GOOGLE_CRASHPAD_BINARY_DIR}/build.ninja)

# NOTE: Partial dependency checking.
# It's not enough to mark the ninja file as the only byproduct,
# as there's many more files needed by ninja after a configure step to produce a correct build.
# For the sake simplicity, I'm not going to bother making that step bulletproof.
add_custom_command(OUTPUT ${GOOGLE_CRASHPAD_NINJA_FILE}
    COMMENT "Configure google crashpad handler"
    DEPENDS crashpad_configure_repo
    WORKING_DIRECTORY ${GOOGLE_CRASHPAD_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GOOGLE_CRASHPAD_BINARY_DIR}
    COMMAND ${GOOGLE_DEPOT_TOOLS_GN} gen ${GOOGLE_CRASHPAD_BINARY_DIR} "--args=is_debug=${GN_IS_DEBUG} extra_cflags=\"${GN_LINK_FLAG}\""
    VERBATIM)

add_custom_target(crashpad_configure_handler DEPENDS ${GOOGLE_CRASHPAD_NINJA_FILE})

if(WIN32)
    set(GOOGLE_CRASHPAD_HANDLER ${GOOGLE_CRASHPAD_BINARY_DIR}/crashpad_handler.exe)
else()
    set(GOOGLE_CRASHPAD_HANDLER ${GOOGLE_CRASHPAD_BINARY_DIR}/crashpad_handler)
endif()

add_custom_command(OUTPUT ${GOOGLE_CRASHPAD_CLIENT_LIBRARIES} ${GOOGLE_CRASHPAD_HANDLER}
    COMMENT "Compile google crashpad handler"
    DEPENDS crashpad_configure_handler
    COMMAND ${GOOGLE_CRASHPAD_NINJA_EXE} -C ${GOOGLE_CRASHPAD_BINARY_DIR} crashpad_handler
    VERBATIM)

add_custom_target(crashpad_compile_handler DEPENDS ${GOOGLE_CRASHPAD_CLIENT_LIBRARIES})

add_library(google_crashpad_client INTERFACE)

target_include_directories(google_crashpad_client SYSTEM INTERFACE
    ${GOOGLE_CRASHPAD_PATH}
    ${GOOGLE_CRASHPAD_PATH}/third_party/mini_chromium/mini_chromium)

add_dependencies(google_crashpad_client crashpad_compile_handler)
target_link_libraries(google_crashpad_client INTERFACE ${GOOGLE_CRASHPAD_CLIENT_LIBRARIES})
target_include_directories(google_crashpad_client SYSTEM INTERFACE ${GOOGLE_CRASHPAD_BINARY_DIR}/gen)

# FIXME We probably shouldn't reference a build dir
install(FILES ${GOOGLE_CRASHPAD_HANDLER} DESTINATION ${CMAKE_INSTALL_PREFIX}/build/external/crashpad)
