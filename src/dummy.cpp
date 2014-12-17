
const char* GL_type_to_string (GLenum type) {
    switch (type) {
        case GL_BOOL: return "bool";
        case GL_INT: return "int";
        case GL_FLOAT: return "float";
        case GL_FLOAT_VEC2: return "vec2";
        case GL_FLOAT_VEC3: return "vec3";
        case GL_FLOAT_VEC4: return "vec4";
        case GL_FLOAT_MAT2: return "mat2";
        case GL_FLOAT_MAT3: return "mat3";
        case GL_FLOAT_MAT4: return "mat4";
        case GL_SAMPLER_2D: return "sampler2D";
        case GL_SAMPLER_3D: return "sampler3D";
        case GL_SAMPLER_CUBE: return "samplerCube";
        case GL_SAMPLER_2D_SHADOW: return "sampler2DShadow";
        default: break;
    }
    return "other";
}

void print_all (GLuint programme) {
    printf ("--------------------\nshader programme %i info:\n", programme);
    int params = -1;
    glGetProgramiv (programme, GL_LINK_STATUS, &params);
    printf ("GL_LINK_STATUS = %i\n", params);

    glGetProgramiv (programme, GL_ATTACHED_SHADERS, &params);
    printf ("GL_ATTACHED_SHADERS = %i\n", params);

    glGetProgramiv (programme, GL_ACTIVE_ATTRIBUTES, &params);
    printf ("GL_ACTIVE_ATTRIBUTES = %i\n", params);
    for (int i = 0; i < params; i++) {
        char name[64];
        int max_length = 64;
        int actual_length = 0;
        int size = 0;
        GLenum type;
        glGetActiveAttrib (programme, i, max_length, &actual_length, &size, &type, name);
        if (size > 1) {
            for (int j = 0; j < size; j++) {
                char long_name[64];
                sprintf (long_name, "%s[%i]", name, j);
                int location = glGetAttribLocation (programme, long_name);
                printf ("  %i) type:%s name:%s location:%i\n",
                        i, GL_type_to_string (type), long_name, location);
            }
        } else {
            int location = glGetAttribLocation (programme, name);
            printf ("  %i) type:%s name:%s location:%i\n",
                    i, GL_type_to_string (type), name, location);
        }
    }

    glGetProgramiv (programme, GL_ACTIVE_UNIFORMS, &params);
    printf ("GL_ACTIVE_UNIFORMS = %i\n", params);
    for (int i = 0; i < params; i++) {
        char name[64];
        int max_length = 64;
        int actual_length = 0;
        int size = 0;
        GLenum type;
        glGetActiveUniform (programme, i, max_length, &actual_length, &size, &type, name);
        if (size > 1) {
            for (int j = 0; j < size; j++) {
                char long_name[64];
                sprintf (long_name, "%s[%i]", name, j);
                int location = glGetUniformLocation (programme, long_name);
                printf ("  %i) type:%s name:%s location:%i\n",
                        i, GL_type_to_string (type), long_name, location);
            }
        } else {
            int location = glGetUniformLocation (programme, name);
            printf ("  %i) type:%s name:%s location:%i\n",
                    i, GL_type_to_string (type), name, location);
        }
    }
}
