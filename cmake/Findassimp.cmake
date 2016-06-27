if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ASSIMP_ARCHITECTURE "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ASSIMP_ARCHITECTURE "32")
endif()

if(WIN32)
    set(ASSIMP_ROOT_DIR CACHE PATH "ASSIMP root directory")

    # Find path of each library
    find_path(ASSIMP_INCLUDE_DIRS
        NAMES assimp/anim.h
        HINTS ${ASSIMP_ROOT_DIR}/include)

    if(MSVC12)
        set(ASSIMP_MSVC_VERSION "vc120")
    elseif(MSVC14)
        set(ASSIMP_MSVC_VERSION "vc140")
    else()
        message(FATAL_ERROR "Unrecognised msvc version")
    endif()

    if(MSVC12 OR MSVC14)
        find_path(ASSIMP_LIBRARY_DIR
            NAMES assimp-${ASSIMP_MSVC_VERSION}-mt.lib
            HINTS ${ASSIMP_ROOT_DIR}/lib${ASSIMP_ARCHITECTURE})

        find_library(ASSIMP_LIBRARY_RELEASE     assimp-${ASSIMP_MSVC_VERSION}-mt.lib     PATHS ${ASSIMP_LIBRARY_DIR})
        find_library(ASSIMP_LIBRARY_DEBUG       assimp-${ASSIMP_MSVC_VERSION}-mtd.lib    PATHS ${ASSIMP_LIBRARY_DIR})

        #set(ASSIMP_SHARED_LIBRARY_RELEASE      ${ASSIMP_ROOT_DIR}/bin${ASSIMP_ARCHITECTURE}/assimp-${ASSIMP_MSVC_VERSION}-mt.dll)
        #set(ASSIMP_SHARED_LIBRARY_DEBUG        ${ASSIMP_ROOT_DIR}/bin${ASSIMP_ARCHITECTURE}/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll)

        set(ASSIMP_LIBRARY optimized ${ASSIMP_LIBRARY_RELEASE} debug ${ASSIMP_LIBRARY_DEBUG} )
        set(ASSIMP_LIBRARIES "ASSIMP_LIBRARY_RELEASE" "ASSIMP_LIBRARY_DEBUG")

        function(ASSIMP_COPY_BINARIES TargetDirectory)
            add_custom_target(AssimpCopyBinaries
                COMMAND ${CMAKE_COMMAND} -E copy ${ASSIMP_ROOT_DIR}/bin${ASSIMP_ARCHITECTURE}/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll     ${TargetDirectory}/Debug/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll
                COMMAND ${CMAKE_COMMAND} -E copy ${ASSIMP_ROOT_DIR}/bin${ASSIMP_ARCHITECTURE}/assimp-${ASSIMP_MSVC_VERSION}-mt.dll         ${TargetDirectory}/Release/assimp-${ASSIMP_MSVC_VERSION}-mt.dll
            COMMENT "Copying Assimp binaries to '${TargetDirectory}'"
            VERBATIM)
        endfunction()
    endif()
else()
    find_path(ASSIMP_INCLUDE_DIRS
        NAMES postprocess.h scene.h version.h config.h cimport.h
        PATHS /usr/local/include/assimp/
        PATHS /usr/include/assimp/)

    find_library(ASSIMP_LIBRARIES
        NAMES assimp
        PATHS /usr/local/lib/
        PATHS /usr/lib/)

    set(ASSIMP_LIBRARY ${ASSIMP_LIBRARIES})

    if(ASSIMP_INCLUDE_DIRS AND ASSIMP_LIBRARIES)
        set(ASSIMP_FOUND TRUE)
    endif()

    if(ASSIMP_FOUND)
        if (NOT ASSIMP_FIND_QUIETLY)
            message(STATUS "Found asset importer library: ${ASSIMP_LIBRARIES}")
        endif (NOT ASSIMP_FIND_QUIETLY)
    else()
        if(ASSIMP_FIND_REQUIRED)
            message(FATAL_ERROR "Could not find asset importer library")
        endif()
    endif()
endif()
