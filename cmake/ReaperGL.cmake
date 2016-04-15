include(${CMAKE_SOURCE_DIR}/cmake/compiler/msvc.cmake)

set(CXX_STANDARD_REQUIRED ON)

set(MSVC_DEBUG_FLAGS "/W4")
set(MSVC_RELEASE_FLAGS "/W0")
set(GCC_DEBUG_FLAGS "-Wall" "-Wextra" "-Wundef" "-Wshadow" "-funsigned-char"
        "-Wchar-subscripts" "-Wcast-align" "-Wwrite-strings" "-Wunused" "-Wuninitialized"
        "-Wpointer-arith" "-Wredundant-decls" "-Winline" "-Wformat"
        "-Wformat-security" "-Winit-self" "-Wdeprecated-declarations"
        "-Wmissing-include-dirs" "-Wmissing-declarations")
set(CLANG_DEBUG_FLAGS ${GCC_DEBUG_FLAGS})

macro(add_vs_source_tree input_files path_to_strip)
    foreach(FILE ${input_files})
        get_filename_component(PARENT_DIR "${FILE}" PATH)
        # strip absolute path
        string(REGEX REPLACE "^${path_to_strip}" "" GROUP ${PARENT_DIR})
        # skip src from path
        string(REGEX REPLACE "(\\./)?(src)/?" "" GROUP "${GROUP}")
        # changes /'s to \\'s
        string(REPLACE "/" "\\" GROUP "${GROUP}")
        # group into "Source Files" and "Header Files"
        if ("${FILE}" MATCHES ".*\\.cpp" OR "${FILE}" MATCHES ".*\\.inl")
            set(GROUP "Source Files\\${GROUP}")
        elseif("${FILE}" MATCHES ".*\\.h" OR "${FILE}" MATCHES ".*\\.hpp")
            set(GROUP "Header Files\\${GROUP}")
        else()
            set(GROUP "Other Files\\${GROUP}")
        endif()
        source_group("${GROUP}" FILES "${FILE}")
    endforeach()
endmacro()

macro(add_custom_build_flags target project_label)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 14)
    if(MSVC)
        set_target_properties(${target} PROPERTIES PROJECT_LABEL ${project_label})
        set_target_properties(${target} PROPERTIES COMPILE_DEFINITIONS "GLM_FORCE_CXX03")
        get_target_property(SOURCE_FILES ${target} SOURCES)
        add_vs_source_tree("${SOURCE_FILES}" ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_options(${target} PRIVATE "/MP")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${MSVC_DEBUG_FLAGS}>")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:RELEASE>:${MSVC_RELEASE_FLAGS}>")
    elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${GCC_DEBUG_FLAGS}>")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE "$<$<CONFIG:DEBUG>:${CLANG_DEBUG_FLAGS}>")
    endif()
endmacro()
