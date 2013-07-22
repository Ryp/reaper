#version 330 core
//
// layout(location = 0) in vec3 vertexPosition_modelspace;
// layout(location = 1) in vec3 vertexColor;
//
// out vec3 fragmentColor;
//
// uniform mat4 MVP;
// uniform vec3 COLOR;
//
// void main()
// {
//   gl_Position =  MVP * vec4(vertexPosition_modelspace,1);
//   fragmentColor = COLOR;
// }

// Input vertex data, different for all executions of this shader.
layout (location = 0) in vec3 vertexPosition_modelspace;

void main(){

    gl_Position.xyz = vertexPosition_modelspace;
    gl_Position.w = 1.0;

}

