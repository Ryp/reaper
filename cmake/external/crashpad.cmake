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

# depot_tools used to contain the ninja binary but doesn't anymore, so we need
# to make sure it's in the PATH instead of sourcing it from google's repo.
find_program(EXE_NINJA ninja REQUIRED)

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

# We need to have a debug and a release version of crashpad's code, this functions creates the proper dependency chain for this to happen.
function(crashpad_create_project_dependencies target is_debug)
    if(is_debug)
        set(CONFIG_SUFFIX debug)
    else()
        set(CONFIG_SUFFIX release)
    endif()

    set(GOOGLE_CRASHPAD_BINARY_DIR ${CMAKE_BINARY_DIR}/external/crashpad_${CONFIG_SUFFIX})

    # Lists of the expected output libraries
    if(WIN32)
        list(APPEND GOOGLE_CRASHPAD_CLIENT_LIBRARIES
            ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/client/client.lib
            ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/client/common.lib
            ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/util/util.lib
            ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/third_party/mini_chromium/mini_chromium/base/base.lib)
    elseif(UNIX)
        list(APPEND GOOGLE_CRASHPAD_CLIENT_LIBRARIES
            ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/client/libclient.a
            ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/client/libcommon.a
            ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/util/libutil.a
            ${GOOGLE_CRASHPAD_BINARY_DIR}/obj/third_party/mini_chromium/mini_chromium/base/libbase.a)
    endif()

    set(GN_IS_DEBUG)
    set(GN_LINK_FLAG)

    if(is_debug)
        set(GN_IS_DEBUG true)
    else()
        set(GN_IS_DEBUG false)
    endif()

    if(MSVC)
        if(is_debug)
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

    set(crashpad_configure_handler_target crashpad_configure_handler_${CONFIG_SUFFIX})
    add_custom_target(${crashpad_configure_handler_target} DEPENDS ${GOOGLE_CRASHPAD_NINJA_FILE})

    add_custom_command(OUTPUT ${GOOGLE_CRASHPAD_CLIENT_LIBRARIES}
        COMMENT "Compile google crashpad handler"
        DEPENDS ${crashpad_configure_handler_target}
        COMMAND ${EXE_NINJA} -C ${GOOGLE_CRASHPAD_BINARY_DIR} crashpad_handler
        VERBATIM)

    set(crashpad_compile_handler_target crashpad_compile_handler_${CONFIG_SUFFIX})
    add_custom_target(${crashpad_compile_handler_target} DEPENDS ${GOOGLE_CRASHPAD_CLIENT_LIBRARIES})

    # This CMake call doesn't support generator expressions, so in the case of multi-config project generators,
    # we end up depending on both debug and release builds, but later we only link the one we use.
    add_dependencies(${target} ${crashpad_compile_handler_target})

    if(is_debug)
        target_link_libraries(${target} INTERFACE $<$<CONFIG:Debug>:${GOOGLE_CRASHPAD_CLIENT_LIBRARIES}>)
        target_include_directories(${target} SYSTEM INTERFACE $<$<CONFIG:Debug>:${GOOGLE_CRASHPAD_BINARY_DIR}/gen>)
    else()
        target_link_libraries(${target} INTERFACE $<$<CONFIG:Release>:${GOOGLE_CRASHPAD_CLIENT_LIBRARIES}>)
        target_include_directories(${target} SYSTEM INTERFACE $<$<CONFIG:Release>:${GOOGLE_CRASHPAD_BINARY_DIR}/gen>)
    endif()
endfunction()

add_library(google_crashpad_client INTERFACE)

target_include_directories(google_crashpad_client SYSTEM INTERFACE
    ${GOOGLE_CRASHPAD_PATH}
    ${GOOGLE_CRASHPAD_PATH}/third_party/mini_chromium/mini_chromium)

if(REAPER_MULTI_CONFIG)
    crashpad_create_project_dependencies(google_crashpad_client ON)
    crashpad_create_project_dependencies(google_crashpad_client OFF)
else()
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        crashpad_create_project_dependencies(google_crashpad_client ON)
    else()
        crashpad_create_project_dependencies(google_crashpad_client OFF)
    endif()
endif()
