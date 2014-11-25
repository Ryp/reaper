set(REAPER_BIN reaper)

set(REAPER_SRCS
    glcontext.cpp
    main.cpp
    AController.cpp
    Camera.cpp
    SixAxis.cpp
    Model.cpp
    ModelLoader.cpp
)

find_package(MoGL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw REQUIRED)
find_package(glbinding REQUIRED)

add_executable(${REAPER_BIN} ${REAPER_SRCS})

#Set compilation flags
set(REAPER_COMPILE_FLAGS "${CMAKE_CXX_FLAGS}")
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(REAPER_COMPILE_FLAGS "${REAPER_COMPILE_FLAGS} -Wall -Wextra -Wshadow -Wundef")
endif()
set_target_properties(${REAPER_BIN} PROPERTIES COMPILE_FLAGS "${REAPER_COMPILE_FLAGS}")

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_BIN} SYSTEM PUBLIC
    ${OPENGL_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${MOGL_INCLUDE_DIRS}
    ${GLBINDING_INCLUDES})

#Libraries
target_link_libraries(${REAPER_BIN}
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARY}
    ${GLBINDING_LIBRARIES})
