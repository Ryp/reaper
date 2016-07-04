
macro(reaper_add_test testname)
    set(REAPER_TEST_BIN test_${testname})
    set(REAPER_TEST_SRCS ${CMAKE_SOURCE_DIR}/test/test_${testname}.cpp)

    add_executable(${REAPER_TEST_BIN} ${REAPER_TEST_SRCS})
    reaper_add_custom_build_flags(${REAPER_TEST_BIN} "${testname}")

    # User includes dirs (won't hide warnings)
    target_include_directories(${REAPER_TEST_BIN} PUBLIC
        ${CMAKE_SOURCE_DIR}/src)

    # External includes dirs (won't show warnings)
    target_include_directories(${REAPER_TEST_BIN} SYSTEM PUBLIC
        ${CMAKE_SOURCE_DIR}/external)

    target_link_libraries(${REAPER_TEST_BIN}
        ${REAPER_PLATFORM_LIBRARIES}
        ${REAPER_CORE_BIN})

    # Register wih ctest
    add_test(NAME ${testname} COMMAND $<TARGET_FILE:${REAPER_TEST_BIN}>)

    if (WIN32)
        set_target_properties(${REAPER_TEST_BIN} PROPERTIES FOLDER Test)
        reaper_vs_set_debugger_flags(${REAPER_TEST_BIN})
    endif()
endmacro()

reaper_add_test(alignment)
reaper_add_test(cache)
reaper_add_test(buddy_allocator)
reaper_add_test(stack_allocator)
