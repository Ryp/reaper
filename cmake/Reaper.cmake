include(${CMAKE_SOURCE_DIR}/cmake/Platform.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/Compiler.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/glslang.cmake)

set(CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})

# Ignore level-4 warning C4201: nonstandard extension used : nameless struct/union
# Ignore level-1 warning C4251: 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
set(REAPER_MSVC_DEBUG_FLAGS "/W4" "/wd4201" "/wd4251")
set(REAPER_MSVC_RELEASE_FLAGS "/W0")
set(REAPER_GCC_DEBUG_FLAGS "-Wall" "-Wextra" "-Wundef" "-Wshadow" "-funsigned-char"
        "-Wchar-subscripts" "-Wcast-align" "-Wwrite-strings" "-Wunused" "-Wuninitialized"
        "-Wpointer-arith" "-Wredundant-decls" "-Winline" "-Wformat"
        "-Wformat-security" "-Winit-self" "-Wdeprecated-declarations"
        "-Wmissing-include-dirs" "-Wmissing-declarations")
set(REAPER_GCC_RELEASE_FLAGS "")

# Use the same flags as GCC
set(REAPER_CLANG_DEBUG_FLAGS ${REAPER_GCC_DEBUG_FLAGS})
set(REAPER_CLANG_RELEASE_FLAGS ${REAPER_GCC_RELEASE_FLAGS})

# Use a xxx.vcxproj.user template file to fill the debugging directory
macro(reaper_vs_set_debugger_flags target)
    set(REAPER_MSVC_DEBUGGER_WORKING_DIR ${CMAKE_SOURCE_DIR})
    configure_file(${CMAKE_SOURCE_DIR}/cmake/vs/template.vcxproj.user ${CMAKE_CURRENT_BINARY_DIR}/${target}.vcxproj.user @ONLY)
endmacro()

# Sort sources in folders for visual studio
macro(reaper_add_vs_source_tree input_files path_to_strip)
    foreach(FILE ${input_files})
        file(RELATIVE_PATH GROUP ${path_to_strip} ${FILE})
        get_filename_component(GROUP ${GROUP} PATH)
        # skip src from path
        string(REGEX REPLACE "(\\./)?(src)/?" "" GROUP "${GROUP}")
        # changes /'s to \\'s
        string(REPLACE "/" "\\" GROUP "${GROUP}")
        # group into "Source Files" and "Header Files"
        if ("${FILE}" MATCHES ".*\\.cpp" OR "${FILE}" MATCHES ".*\\.inl" OR "${FILE}" MATCHES ".*\\.h" OR "${FILE}" MATCHES ".*\\.hpp")
            set(GROUP "Source Files\\${GROUP}")
        #elseif("${FILE}" MATCHES ".*\\.h" OR "${FILE}" MATCHES ".*\\.hpp")
        #    set(GROUP "Header Files\\${GROUP}")
        else()
            set(GROUP "Other Files\\${GROUP}")
        endif()
        source_group("${GROUP}" FILES "${FILE}")
    endforeach()
endmacro()

# Add relevant warnings to the specified target
macro(reaper_add_custom_build_flags target project_label)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 14)
    set(TARGET_COMPILE_DEFINITIONS REAPER_BUILD_${REAPER_BUILD_TYPE})
    if(MSVC)
        set_target_properties(${target} PROPERTIES PROJECT_LABEL ${project_label})
        set(TARGET_COMPILE_DEFINITIONS ${TARGET_COMPILE_DEFINITIONS} GLM_FORCE_CXX03)
        get_target_property(SOURCE_FILES ${target} SOURCES)
        reaper_add_vs_source_tree("${SOURCE_FILES}" ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_options(${target} PRIVATE "/MP")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_MSVC_DEBUG_FLAGS}>")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${REAPER_MSVC_RELEASE_FLAGS}>")
    elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(${target} PRIVATE "-fvisibility=hidden")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_GCC_DEBUG_FLAGS}>")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${REAPER_GCC_RELEASE_FLAGS}>")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE "-fvisibility=hidden")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_CLANG_DEBUG_FLAGS}>")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${REAPER_CLANG_RELEASE_FLAGS}>")
    endif()
    target_compile_definitions(${target} PRIVATE ${TARGET_COMPILE_DEFINITIONS})
endmacro()

# Reaper standard test macro
macro(reaper_add_tests library testfiles)
    set(REAPER_TEST_FILES ${testfiles} ${ARGN}) # Expecting a list here
    set(REAPER_TEST_BIN ${library}_tests)
    set(REAPER_TEST_SRCS ${REAPER_TEST_FILES})

    add_executable(${REAPER_TEST_BIN} ${REAPER_TEST_SRCS})
    reaper_add_custom_build_flags(${REAPER_TEST_BIN} "${REAPER_TEST_BIN}")

    # User includes dirs (won't hide warnings)
    target_include_directories(${REAPER_TEST_BIN} PUBLIC
        ${CMAKE_SOURCE_DIR}/src)

    # External includes dirs (won't show warnings)
    target_include_directories(${REAPER_TEST_BIN} SYSTEM PUBLIC
        ${CMAKE_SOURCE_DIR}/external/doctest)

    target_link_libraries(${REAPER_TEST_BIN} ${library})

    # Register wih ctest
    add_test(NAME ${REAPER_TEST_BIN} COMMAND $<TARGET_FILE:${REAPER_TEST_BIN}>)

    if (WIN32)
        set_target_properties(${REAPER_TEST_BIN} PROPERTIES FOLDER Test)
        reaper_vs_set_debugger_flags(${REAPER_TEST_BIN})
    endif()
endmacro()
