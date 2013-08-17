
set ( SRCS main.cpp
      Debug.cpp GLContext.cpp GLErrorLogger.cpp ReaperCore.cpp Reaper.cpp
      Renderer/PostProcessor.cpp 
      Camera/Camera.cpp  
      Model/Model.cpp Model/ModelLoader.cpp
      Shader/ShaderProgram.cpp Shader/ShaderObject.cpp
      Joystick/AController.cpp Joystick/SixAxis.cpp
    )
    
add_executable ( reaper ${SRCS} )
target_link_libraries ( reaper ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${GLFW_LIBRARY} )
install ( TARGETS reaper DESTINATION bin )
        