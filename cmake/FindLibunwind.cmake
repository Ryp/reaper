# Find the libunwind library
#
#  LIBUNWIND_FOUND       - True if libunwind was found.
#  LIBUNWIND_LIBRARIES   - The libraries needed to use libunwind
#  LIBUNWIND_INCLUDE_DIR - Location of unwind.h and libunwind.h
 
find_path(LIBUNWIND_INCLUDE_DIR libunwind.h)
if(NOT LIBUNWIND_INCLUDE_DIR)
  message(FATAL_ERROR "failed to find libunwind.h")
elif(NOT EXISTS "${LIBUNWIND_INCLUDE_DIR}/unwind.h")
  message(FATAL_ERROR "libunwind.h was found, but unwind.h was not found in that directory.")
  SET(LIBUNWIND_INCLUDE_DIR "")
endif()
 
find_library(LIBUNWIND_GENERIC_LIBRARY "unwind")
if (NOT LIBUNWIND_GENERIC_LIBRARY)
    MESSAGE(FATAL_ERROR "failed to find unwind generic library")
endif ()
set(LIBUNWIND_LIBRARIES ${LIBUNWIND_GENERIC_LIBRARY})

# For some reason, we have to link to two libunwind shared object files:
# one arch-specific and one not.
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    set(LIBUNWIND_ARCH "arm")
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    set(LIBUNWIND_ARCH "x86_64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
    set(LIBUNWIND_ARCH "x86")
endif()

if (LIBUNWIND_ARCH)
    find_library(LIBUNWIND_SPECIFIC_LIBRARY "unwind-${LIBUNWIND_ARCH}")
    if (NOT LIBUNWIND_SPECIFIC_LIBRARY)
        message(FATAL_ERROR "failed to find unwind-${LIBUNWIND_ARCH}")
    endif ()
    set(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIBRARIES} ${LIBUNWIND_SPECIFIC_LIBRARY})
endif(LIBUNWIND_ARCH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libunwind DEFAULT_MSG LIBUNWIND_INCLUDE_DIR LIBUNWIND_LIBRARIES)

mark_as_advanced(LIBUNWIND_INCLUDE_DIR LIBUNWIND_LIBRARIES)

if(Libunwind_FOUND AND NOT TARGET Libunwind::Libunwind)
    add_library(Libunwind::Libunwind INTERFACE IMPORTED)
    set_target_properties(Libunwind::Libunwind PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${LIBUNWIND_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${LIBUNWIND_LIBRARIES}")
endif()
