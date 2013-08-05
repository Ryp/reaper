TARGET		=	reaper

CXX		=	g++

CXXFLAGS	+=	-Wall -Wextra -ansi -pedantic -std=c++11 -Wshadow

IWDIR		=	./include

LD		=	g++

LDFLAGS		+=	$(LIBS) $(LPATH)

LIBS		=	-lglfw -lGLEW -lGL

LPATH		=

IPATH		=	-I./include				\
			-I./src

RM		=	rm -f

SRC		=	src/main.cpp				\
			src/Debug.cpp				\
			src/GLContext.cpp			\
			src/GLErrorLogger.cpp			\
			src/ReaperCore.cpp			\
			src/Reaper.cpp				\
			src/Model/Model.cpp			\
 			src/Model/ModelLoader.cpp		\
			src/Shader/ShaderProgram.cpp		\
			src/Shader/ShaderObject.cpp		\
			src/Joystick/AController.cpp		\
			src/Joystick/SixAxis.cpp

OBJ		=	$(SRC:.cpp=.o)

$(TARGET):		$(OBJ)
			$(LD) $(LDFLAGS) $(OBJ) -o $(TARGET)

all:			$(TARGET)

%.o:			%.cpp
			$(CXX) $(CXXFLAGS) $(IPATH) -isystem $(IWDIR) -c $< -o $@

clean:
			@$(RM) $(OBJ)

fclean:			clean
			@$(RM) $(TARGET)

re:			fclean all

.PHONY:			all clean fclean re
