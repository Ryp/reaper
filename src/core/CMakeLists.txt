if(UNIX)
    find_package(Libunwind REQUIRED)
    set(REAPER_CORE_LIBRARIES ${LIBUNWIND_LIBRARIES} -lpthread -ldl)
    set(REAPER_CORE_INCLUDES ${LIBUNWIND_INCLUDE_DIR})
endif()

set(REAPER_CORE_BIN reaper_core)
set(REAPER_CORE_BIN ${REAPER_CORE_BIN} PARENT_SCOPE)

set(REAPER_CORE_DIR ${CMAKE_SOURCE_DIR}/src/core)
set(REAPER_CORE_SRCS
    ${REAPER_CORE_DIR}/Assert.cpp
    ${REAPER_CORE_DIR}/Assert.h
    ${REAPER_CORE_DIR}/BitTricks.h
    ${REAPER_CORE_DIR}/Cast.h
    ${REAPER_CORE_DIR}/Compiler.h
    ${REAPER_CORE_DIR}/CoreExport.h
    ${REAPER_CORE_DIR}/Debugger.cpp
    ${REAPER_CORE_DIR}/Debugger.h
    ${REAPER_CORE_DIR}/DynamicLibrary.cpp
    ${REAPER_CORE_DIR}/DynamicLibrary.h
    ${REAPER_CORE_DIR}/EnumHelper.h
    ${REAPER_CORE_DIR}/Platform.h
    ${REAPER_CORE_DIR}/StackTrace.cpp
    ${REAPER_CORE_DIR}/StackTrace.h
    ${REAPER_CORE_DIR}/Types.h

    ${REAPER_CORE_DIR}/fs/FileLoading.cpp
    ${REAPER_CORE_DIR}/fs/FileLoading.h
    ${REAPER_CORE_DIR}/fs/Path.cpp
    ${REAPER_CORE_DIR}/fs/Path.h
#     ${REAPER_CORE_DIR}/fs/UnixFileWatcher.cpp
#     ${REAPER_CORE_DIR}/fs/UnixFileWatcher.h

    ${REAPER_CORE_DIR}/memory/Allocator.cpp
    ${REAPER_CORE_DIR}/memory/Allocator.h
    ${REAPER_CORE_DIR}/memory/BuddyAllocator.cpp
    ${REAPER_CORE_DIR}/memory/BuddyAllocator.h
    ${REAPER_CORE_DIR}/memory/CacheLine.cpp
    ${REAPER_CORE_DIR}/memory/CacheLine.h
    ${REAPER_CORE_DIR}/memory/LinearAllocator.cpp
    ${REAPER_CORE_DIR}/memory/LinearAllocator.h
    ${REAPER_CORE_DIR}/memory/StackAllocator.cpp
    ${REAPER_CORE_DIR}/memory/StackAllocator.h
)

add_library(${REAPER_CORE_BIN} ${REAPER_BUILD_TYPE} ${REAPER_CORE_SRCS})
reaper_add_custom_build_flags(${REAPER_CORE_BIN} "Core")

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_CORE_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_CORE_BIN} SYSTEM PUBLIC
    ${REAPER_CORE_INCLUDES})

#Libraries
target_link_libraries(${REAPER_CORE_BIN} ${REAPER_CORE_LIBRARIES})

set_target_properties(${REAPER_CORE_BIN} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ${REAPER_PCH})
cotire(${REAPER_CORE_BIN})

if (REAPER_BUILD_TESTS)
    reaper_add_test(${REAPER_CORE_BIN} alignment)
    reaper_add_test(${REAPER_CORE_BIN} cache)
    reaper_add_test(${REAPER_CORE_BIN} buddy_allocator)
    reaper_add_test(${REAPER_CORE_BIN} stack_allocator)
endif()
