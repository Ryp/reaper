include(${CMAKE_SOURCE_DIR}/cmake/compiler/msvc.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/platform/unix.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/platform/windows.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/glslang.cmake)

set(CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${Reaper_BINARY_DIR})

set(REAPER_MSVC_DEBUG_FLAGS "/W4")
set(REAPER_MSVC_RELEASE_FLAGS "/W0")
set(REAPER_GCC_DEBUG_FLAGS "-Wall" "-Wextra" "-Wundef" "-Wshadow" "-funsigned-char"
        "-Wchar-subscripts" "-Wcast-align" "-Wwrite-strings" "-Wunused" "-Wuninitialized"
        "-Wpointer-arith" "-Wredundant-decls" "-Winline" "-Wformat"
        "-Wformat-security" "-Winit-self" "-Wdeprecated-declarations"
        "-Wmissing-include-dirs" "-Wmissing-declarations")
set(REAPER_CLANG_DEBUG_FLAGS ${REAPER_GCC_DEBUG_FLAGS})

# Use a xxx.vcxproj.user template file to fill the debugging directory
macro(reaper_vs_set_debugger_flags target)
    set(REAPER_MSVC_DEBUGGER_WORKING_DIR ${CMAKE_SOURCE_DIR})
    configure_file(${CMAKE_SOURCE_DIR}/cmake/platform/template.vcxproj.user ${CMAKE_CURRENT_BINARY_DIR}/${target}.vcxproj.user @ONLY)
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
    if(MSVC)
        set_target_properties(${target} PROPERTIES PROJECT_LABEL ${project_label})
        set_target_properties(${target} PROPERTIES COMPILE_DEFINITIONS "GLM_FORCE_CXX03")
        get_target_property(SOURCE_FILES ${target} SOURCES)
        reaper_add_vs_source_tree("${SOURCE_FILES}" ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_options(${target} PRIVATE "/MP")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_MSVC_DEBUG_FLAGS}>")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${REAPER_MSVC_RELEASE_FLAGS}>")
    elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(${target} PRIVATE "-fvisibility=hidden")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_GCC_DEBUG_FLAGS}>")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE "-fvisibility=hidden")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${REAPER_CLANG_DEBUG_FLAGS}>")
    endif()
endmacro()
