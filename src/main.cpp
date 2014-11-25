#include <fstream>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace gl;

#include "glcontext.hpp"

#include <mogl/exception/moglexception.hpp>
#include <mogl/shader.hpp>
#include <mogl/texture.hpp>
#include <mogl/framebuffer.hpp>
#include <mogl/renderbuffer.hpp>
#include <mogl/buffer/bufferobject.hpp>
#include <mogl/queryobject.hpp>
#include <mogl/debug.hpp>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

#include "ModelLoader.hh"
#include "SixAxis.hpp"
#include "Camera.hpp"

static const GLfloat quadVertexBufferData[] = {
    -1.0f, -1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    1.0f,  1.0f, 0.0f,
};

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

static void loadSimpleShader(mogl::ShaderProgram& shader, const std::string& vertexFile, const std::string& fragmentFile)
{
    std::ifstream       vsFile(vertexFile);
    std::ifstream       fsFile(fragmentFile);
    mogl::ShaderObject  vertex(vsFile, mogl::ShaderObject::ShaderType::VertexShader);
    mogl::ShaderObject  fragment(fsFile, mogl::ShaderObject::ShaderType::FragmentShader);

    if (!vertex.compile())
        throw(std::runtime_error(vertex.getLog()));
    if (!fragment.compile())
        throw(std::runtime_error(fragment.getLog()));

    shader.attach(vertex);
    shader.attach(fragment);

    if (!shader.link())
        throw(std::runtime_error(shader.getLog()));
}

void    hdr_test(GLContext& ctx, SixAxis& controller)
{
    float                       frameTime;
    Camera                      cam(glm::vec3(0.0, 3.0, -5.0));
    mogl::ShaderProgram         shader;
    mogl::ShaderProgram         postshader;
    mogl::ShaderProgram         depthpassshader;
    mogl::BufferObject          quadBuffer(GL_ARRAY_BUFFER);
    mogl::BufferObject          teapotVBuffer(GL_ARRAY_BUFFER);
    mogl::BufferObject          teapotNBuffer(GL_ARRAY_BUFFER);
    mogl::FrameBufferObject     postFrameBuffer;
    mogl::FrameBufferObject     depthFrameBuffer;
    mogl::TextureObject         frameTexture(GL_TEXTURE_2D);
    mogl::TextureObject         depthTexture(GL_TEXTURE_2D);
    mogl::RenderBufferObject    postDepthBuffer;
    ModelLoader                 loader;
    Model*                      model = loader.load("rc/model/teapot.obj");
    mogl::QueryObject           timeQuery(GL_TIME_ELAPSED);
    glm::mat4                   biasMatrix(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0,
        0.5, 0.5, 0.5, 1.0
    );

    model->debugDump();
    loadSimpleShader(depthpassshader, "rc/shader/depth_pass.vert", "rc/shader/depth_pass.frag");
    loadSimpleShader(shader, "rc/shader/shadow.vert", "rc/shader/shadow.frag");
    loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/tonemapping.frag");

    depthpassshader.printDebug();
    shader.printDebug();
    postshader.printDebug();
    print_all(postshader.getHandle());

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glClearColor(0.2f, 0.5f, 0.0f, 0.0f);

    quadBuffer.bind();
    quadBuffer.setData(sizeof(quadVertexBufferData), quadVertexBufferData, GL_STATIC_DRAW);

    teapotVBuffer.bind();
    teapotVBuffer.setData(model->getVertexBufferSize(), model->getVertexBuffer(), GL_STATIC_DRAW);

    teapotNBuffer.bind();
    teapotNBuffer.setData(model->getNormalBufferSize(), model->getNormalBuffer(), GL_STATIC_DRAW);

    postFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);

    glActiveTexture(GL_TEXTURE0);
    frameTexture.bind();
    frameTexture.setImage2D(0, static_cast<GLint>(GL_RGB16F), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_RGB, GL_HALF_FLOAT, 0);
    frameTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    frameTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    frameTexture.setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    frameTexture.setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    mogl::FrameBuffer::setTexture2D(mogl::FrameBuffer::Target::FrameBuffer,
                                    mogl::FrameBuffer::Attachment::Color0,
                                    frameTexture
    );

    postDepthBuffer.bind();
    postDepthBuffer.setStorage(GL_DEPTH_COMPONENT, ctx.getWindowSize().x, ctx.getWindowSize().y);
    mogl::FrameBuffer::setRenderBuffer(mogl::FrameBuffer::Target::FrameBuffer,
                                       mogl::FrameBuffer::Attachment::Depth,
                                       postDepthBuffer
    );

    GLenum DrawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);

    if (!mogl::FrameBuffer::isComplete(mogl::FrameBuffer::Target::FrameBuffer))
        throw (std::runtime_error("bad framebuffer"));

    // The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
    depthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);

    // Depth texture. Slower than a depth buffer, but you can sample it later in your shader
    depthTexture.bind();
    depthTexture.setImage2D(0, static_cast<GLint>(GL_DEPTH_COMPONENT16), 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    depthTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    depthTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    depthTexture.setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    depthTexture.setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    depthTexture.setParameter(GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    depthTexture.setParameter(GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture.getHandle(), 0);
    glDrawBuffer(GL_NONE); // No color buffer is drawn to.

    if (!mogl::FrameBuffer::isComplete(mogl::FrameBuffer::Target::FrameBuffer))
        throw (std::runtime_error("bad framebuffer"));

    cam.setRotation(M_PI_2, -0.3f);
    cam.update(Camera::Spherical);
    while (ctx.isOpen())
    {
        frameTime = ctx.getTime();

        controller.update();
        if (controller.isHeld(SixAxis::Buttons::Start))
            break;
        cam.move(controller.getAxis(SixAxis::LeftAnalogY) / -10.0f, controller.getAxis(SixAxis::LeftAnalogX) / -10.0f, 0.0f);
        cam.rotate(controller.getAxis(SixAxis::RightAnalogX) / 10.0f, controller.getAxis(SixAxis::RightAnalogY) / -10.0f);
        cam.update(Camera::Spherical);
        if (controller.isPressed(SixAxis::Buttons::Select))
            cam.reset();

        glm::mat4           Projection  = glm::perspective(45.0f, static_cast<float>(ctx.getWindowSize().x) / static_cast<float>(ctx.getWindowSize().y), 0.1f, 100.0f);
        glm::mat4           View        = cam.getViewMatrix();
        glm::mat4           Model       = glm::mat4(1.0);
        glm::mat4           MV          = View * Model;
        glm::mat4           MVP         = Projection * MV;
        glm::vec3           lightInvDir = glm::vec3(0.5f,2,2);
        glm::mat4           depthProjectionMatrix = glm::ortho<float>(-10,10,-10,10,-10,20);
        glm::mat4           depthViewMatrix = glm::lookAt(lightInvDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4           depthModelMatrix = Model;
        glm::mat4           depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
        glm::mat4           depthBiasMVP = biasMatrix * depthMVP;

        timeQuery.begin();

        //Depth buffer pass
        depthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        glViewport(0, 0, 1024, 1024);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        depthpassshader.bind();
        depthpassshader.setUniformMatrixPtr<4>("depthMVP", &depthMVP[0][0]);

        glEnableVertexAttribArray(0);
        teapotVBuffer.bind();
        depthpassshader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);

        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3);
        glDisableVertexAttribArray(0);

        //Render shading with shadow map
        postFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.bind();
        shader.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);
        shader.setUniformMatrixPtr<4>("DepthBiasMVP", &depthBiasMVP[0][0]);
        shader.setUniformMatrixPtr<4>("MV", &MV[0][0]);
        shader.setUniformMatrixPtr<4>("V", &View[0][0]);
        shader.setUniformMatrixPtr<4>("M", &Model[0][0]);
        shader.setUniformPtr<3>("lightInvDirection_worldspace", &lightInvDir[0]);

        glActiveTexture(GL_TEXTURE0);
        depthTexture.bind();
        shader.setUniform<GLint>("shadowMap", 0);

        glEnableVertexAttribArray(0);
        teapotVBuffer.bind();
        shader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);
        glEnableVertexAttribArray(1);
        teapotNBuffer.bind();
        shader.setVertexAttribPointer("vertexNormal_modelspace", 3, GL_FLOAT);
        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);

        //Apply post-process effects
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        postshader.bind();

        glActiveTexture(GL_TEXTURE0);
        frameTexture.bind();

        postshader.setUniform<GLint>("renderedTexture", 0);
        postshader.setUniformSubroutine(mogl::ShaderObject::ShaderType::FragmentShader, "tonemapSelector", "reinhardtonemap");
        if (controller.isHeld(SixAxis::Buttons::Circle))
            postshader.setUniformSubroutine(mogl::ShaderObject::ShaderType::FragmentShader, "tonemapSelector", "uncharted2tonemap");
        else if (controller.isHeld(SixAxis::Buttons::Square))
            postshader.setUniformSubroutine(mogl::ShaderObject::ShaderType::FragmentShader, "tonemapSelector", "notonemap");

        glEnableVertexAttribArray(0);
        quadBuffer.bind();
        postshader.setVertexAttribPointer("v_coord", 3, GL_FLOAT);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);

        timeQuery.end();
        double glms = static_cast<double>(timeQuery.getResult<GLuint>(GL_QUERY_RESULT)) * 0.000001;

        ctx.swapBuffers();
        frameTime = ctx.getTime() - frameTime;
        ctx.setTitle(std::to_string(frameTime * 1000.0f) + " ms / " + std::to_string(glms) + " ms " + std::to_string(ctx.getWindowSize().x) + "x"  + std::to_string(ctx.getWindowSize().y));
    }
}

void    pcf_test(GLContext& ctx, SixAxis& controller)
{
    float                       frameTime;
    Camera                      cam(glm::vec3(0.0, 3.0, -5.0));
    mogl::ShaderProgram         shader;
    mogl::ShaderProgram         postshader;
    mogl::ShaderProgram         depthpassshader;
    mogl::BufferObject          quadBuffer(GL_ARRAY_BUFFER);
    mogl::BufferObject          teapotVBuffer(GL_ARRAY_BUFFER);
    mogl::BufferObject          teapotNBuffer(GL_ARRAY_BUFFER);
    mogl::FrameBufferObject     postFrameBuffer;
    mogl::FrameBufferObject     depthFrameBuffer;
    mogl::TextureObject         frameTexture(GL_TEXTURE_2D);
    mogl::TextureObject         depthTexture(GL_TEXTURE_2D);
    mogl::RenderBufferObject    postDepthBuffer;
    ModelLoader                 loader;
    Model*                      model = loader.load("rc/model/teapot.obj");
    mogl::QueryObject           timeQuery(GL_TIME_ELAPSED);
    glm::mat4                   biasMatrix(
                                    0.5, 0.0, 0.0, 0.0,
                                    0.0, 0.5, 0.0, 0.0,
                                    0.0, 0.0, 0.5, 0.0,
                                    0.5, 0.5, 0.5, 1.0
                                );

    model->debugDump();
    loadSimpleShader(depthpassshader, "rc/shader/depth_pass.vert", "rc/shader/depth_pass.frag");
    loadSimpleShader(shader, "rc/shader/shadow.vert", "rc/shader/shadow.frag");
    loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/fxaa.frag");

    depthpassshader.printDebug();
    shader.printDebug();
    postshader.printDebug();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glClearColor(0.2f, 0.5f, 0.0f, 0.0f);

    quadBuffer.bind();
    quadBuffer.setData(sizeof(quadVertexBufferData), quadVertexBufferData, GL_STATIC_DRAW);

    teapotVBuffer.bind();
    teapotVBuffer.setData(model->getVertexBufferSize(), model->getVertexBuffer(), GL_STATIC_DRAW);

    teapotNBuffer.bind();
    teapotNBuffer.setData(model->getNormalBufferSize(), model->getNormalBuffer(), GL_STATIC_DRAW);

    postFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);

    glActiveTexture(GL_TEXTURE0);
    frameTexture.bind();
    frameTexture.setImage2D(0, static_cast<GLint>(GL_RGB), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    frameTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    frameTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    frameTexture.setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    frameTexture.setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    mogl::FrameBuffer::setTexture2D(mogl::FrameBuffer::Target::FrameBuffer,
                                    mogl::FrameBuffer::Attachment::Color0,
                                    frameTexture
                                   );

    postDepthBuffer.bind();
    postDepthBuffer.setStorage(GL_DEPTH_COMPONENT, ctx.getWindowSize().x, ctx.getWindowSize().y);
    mogl::FrameBuffer::setRenderBuffer(mogl::FrameBuffer::Target::FrameBuffer,
                                       mogl::FrameBuffer::Attachment::Depth,
                                       postDepthBuffer
                                      );

    GLenum DrawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);

    if (!mogl::FrameBuffer::isComplete(mogl::FrameBuffer::Target::FrameBuffer))
        throw (std::runtime_error("bad framebuffer"));

    // The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
    depthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);

    // Depth texture. Slower than a depth buffer, but you can sample it later in your shader
    depthTexture.bind();
    depthTexture.setImage2D(0, static_cast<GLint>(GL_DEPTH_COMPONENT16), 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    depthTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    depthTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    depthTexture.setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    depthTexture.setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    depthTexture.setParameter(GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    depthTexture.setParameter(GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture.getHandle(), 0);
    glDrawBuffer(GL_NONE); // No color buffer is drawn to.

    if (!mogl::FrameBuffer::isComplete(mogl::FrameBuffer::Target::FrameBuffer))
        throw (std::runtime_error("bad framebuffer"));

    cam.setRotation(M_PI_2, -0.3f);
    cam.update(Camera::Spherical);
    while (ctx.isOpen())
    {
        frameTime = ctx.getTime();

        controller.update();
        if (controller.isHeld(SixAxis::Buttons::Start))
            break;
        cam.move(controller.getAxis(SixAxis::LeftAnalogY) / -10.0f, controller.getAxis(SixAxis::LeftAnalogX) / -10.0f, 0.0f);
        cam.rotate(controller.getAxis(SixAxis::RightAnalogX) / 10.0f, controller.getAxis(SixAxis::RightAnalogY) / -10.0f);
        cam.update(Camera::Spherical);
        if (controller.isPressed(SixAxis::Buttons::Select))
            cam.reset();

        glm::mat4           Projection  = glm::perspective(45.0f, static_cast<float>(ctx.getWindowSize().x) / static_cast<float>(ctx.getWindowSize().y), 0.1f, 100.0f);
        glm::mat4           View        = cam.getViewMatrix();
        glm::mat4           Model       = glm::mat4(1.0);
        glm::mat4           MV          = View * Model;
        glm::mat4           MVP         = Projection * MV;
        glm::vec3           lightInvDir = glm::vec3(0.5f,2,2);
        glm::mat4           depthProjectionMatrix = glm::ortho<float>(-10,10,-10,10,-10,20);
        glm::mat4           depthViewMatrix = glm::lookAt(lightInvDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4           depthModelMatrix = Model;
        glm::mat4           depthMVP    = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
        glm::mat4           depthBiasMVP = biasMatrix * depthMVP;

        timeQuery.begin();

        //Depth buffer pass
        depthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        glViewport(0, 0, 1024, 1024);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        depthpassshader.bind();
        depthpassshader.setUniformMatrixPtr<4>("depthMVP", &depthMVP[0][0]);

        glEnableVertexAttribArray(0);
        teapotVBuffer.bind();
        depthpassshader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);

        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3);
        glDisableVertexAttribArray(0);

        //Render shading with shadow map
        postFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.bind();
        shader.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);
        shader.setUniformMatrixPtr<4>("DepthBiasMVP", &depthBiasMVP[0][0]);
        shader.setUniformMatrixPtr<4>("MV", &MV[0][0]);
        shader.setUniformMatrixPtr<4>("V", &View[0][0]);
        shader.setUniformMatrixPtr<4>("M", &Model[0][0]);
        shader.setUniformPtr<3>("lightInvDirection_worldspace", &lightInvDir[0]);

        glActiveTexture(GL_TEXTURE0);
        depthTexture.bind();
        shader.setUniform<GLint>("shadowMap", 0);

        glEnableVertexAttribArray(0);
        teapotVBuffer.bind();
        shader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);
        glEnableVertexAttribArray(1);
        teapotNBuffer.bind();
        shader.setVertexAttribPointer("vertexNormal_modelspace", 3, GL_FLOAT);
        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);

        //Apply post-process effects
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        postshader.bind();

        glActiveTexture(GL_TEXTURE0);
        frameTexture.bind();

        postshader.setUniform<GLint>("renderedTexture", 0);
        postshader.setUniform<GLfloat>("frameBufSize", ctx.getWindowSize().x, ctx.getWindowSize().y);

        glEnableVertexAttribArray(0);
        quadBuffer.bind();
        postshader.setVertexAttribPointer("v_coord", 3, GL_FLOAT);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);

        timeQuery.end();
        double glms = static_cast<double>(timeQuery.getResult<GLuint>(GL_QUERY_RESULT)) * 0.000001;

        ctx.swapBuffers();
        frameTime = ctx.getTime() - frameTime;
        ctx.setTitle(std::to_string(frameTime * 1000.0f) + " ms / " + std::to_string(glms) + " ms " + std::to_string(ctx.getWindowSize().x) + "x"  + std::to_string(ctx.getWindowSize().y));
    }
}

int main(int /*ac*/, char** /*av*/)
{
    try {
        GLContext   ctx;
        SixAxis     controller("/dev/input/js0");

        ctx.create(1600, 900, false);
        ctx.printLog(false);
//         pcf_test(ctx, controller);
        hdr_test(ctx, controller);
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
