set(REAPER_BIN reaper)

set(REAPER_SRCS
    main.cpp
    Camera.cpp
    cvar/cvar.cpp
    unixfilewatcher.cpp
    filesystem/path.cpp
    input/AController.cpp
    input/SixAxis.cpp
    imgui/imgui.cpp
    imgui/imgui_impl_glfw_gl3.cpp
    math/spline.cpp
    pipeline/glcontext.cpp
    pipeline/mesh.cpp
    resource/imageloader.cpp
    resource/Model.cpp
    resource/ModelLoader.cpp
    resource/resourcemanager.cpp
    resource/shaderhelper.cpp
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(moGL NO_MODULE REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glbinding REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

add_executable(${REAPER_BIN} ${REAPER_SRCS})

#Set compilation flags
set(REAPER_COMPILE_FLAGS "${CMAKE_CXX_FLAGS}")
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(REAPER_COMPILE_FLAGS "${REAPER_COMPILE_FLAGS} -Wall -Wextra -Wshadow -Wundef")
endif()
set_target_properties(${REAPER_BIN} PROPERTIES
    CXX_STANDARD 14
    COMPILE_FLAGS "${REAPER_COMPILE_FLAGS}")

#User includes dirs (won't hide warnings)
target_include_directories(${REAPER_BIN} PUBLIC
    ${CMAKE_SOURCE_DIR}/src)

#External includes dirs (won't show warnings)
target_include_directories(${REAPER_BIN} SYSTEM PUBLIC
    ${OPENGL_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${MOGL_INCLUDE_DIRS}
    ${GLBINDING_INCLUDES}
    ${OPENEXR_INCLUDE_DIRS})

#Libraries
target_link_libraries(${REAPER_BIN}
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${GLBINDING_LIBRARIES}
    ${OPENEXR_LIBRARIES} -lpthread)
