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
#include <mogl/buffer/vertexarrayobject.hpp>
#include <mogl/queryobject.hpp>
#include <mogl/debug.hpp>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

#include "ModelLoader.hh"
#include "SixAxis.hpp"
#include "Camera.hpp"
#include "spline.hpp"
#include "shaderhelper.hpp"

void    hdr_test(GLContext& ctx, SixAxis& controller)
{
    float                       frameTime;
    Camera                      cam(glm::vec3(0.0, 3.0, -5.0));
    mogl::ShaderProgram         shader;
    mogl::ShaderProgram         postshader;
    mogl::ShaderProgram         normdepthpassshader;
    mogl::ShaderProgram         depthpassshader;
    mogl::VertexArrayObject     vao;
    mogl::BufferObject          quadBuffer(GL_ARRAY_BUFFER);
    mogl::BufferObject          teapotVBuffer(GL_ARRAY_BUFFER);
    mogl::BufferObject          teapotNBuffer(GL_ARRAY_BUFFER);
    mogl::FrameBufferObject     postFrameBuffer;
    mogl::FrameBufferObject     shadowDepthFrameBuffer;
    mogl::FrameBufferObject     screenDepthFrameBuffer;
    mogl::TextureObject         frameTexture(GL_TEXTURE_2D);
    mogl::TextureObject         shadowDepthTexture(GL_TEXTURE_2D);
    mogl::TextureObject         screenDepthTexture(GL_TEXTURE_2D);
    mogl::TextureObject         screenDepthNormalTexture(GL_TEXTURE_2D);
    mogl::RenderBufferObject    postDepthBuffer;
    ModelLoader                 loader;
    Model*                      model = loader.load("rc/model/sibenik.obj");
    Model*                      quad = loader.load("rc/model/quad.obj");
    mogl::QueryObject           timeQuery(GL_TIME_ELAPSED);
    glm::vec3                   translation;
    glm::mat4                   biasMatrix(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0,
        0.5, 0.5, 0.5, 1.0
    );

    model->debugDump();
    ShaderHelper::loadSimpleShader(depthpassshader, "rc/shader/depth_pass.vert", "rc/shader/depth_pass.frag");
    ShaderHelper::loadSimpleShader(normdepthpassshader, "rc/shader/depth_pass_normalized.vert", "rc/shader/depth_pass_normalized.frag");
    ShaderHelper::loadSimpleShader(shader, "rc/shader/shadow_ssao.vert", "rc/shader/shadow_ssao.frag");
    ShaderHelper::loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/fxaa.frag");
//     loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/tonemapping.frag");

    depthpassshader.printDebug();
    normdepthpassshader.printDebug();
    shader.printDebug();
    postshader.printDebug();

    // Frame buffer for post processing
    try {
        postFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);

        glActiveTexture(GL_TEXTURE0); MOGL_ASSERT_GLSTATE();
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
        glDrawBuffers(1, DrawBuffers); MOGL_ASSERT_GLSTATE();
        if (!mogl::FrameBuffer::isComplete(mogl::FrameBuffer::Target::FrameBuffer))
            throw (std::runtime_error("bad framebuffer"));
    }
    catch (const std::runtime_error& e) {
        throw (std::runtime_error(std::string("Main framebuffer error: ") + e.what()));
    }

    // Frame buffer for the shadow map
    try {
        // The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
        shadowDepthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        // Depth texture. Slower than a depth buffer, but you can sample it later in your shader
        shadowDepthTexture.bind();
        shadowDepthTexture.setImage2D(0, static_cast<GLint>(GL_DEPTH_COMPONENT16), 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        shadowDepthTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        shadowDepthTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        shadowDepthTexture.setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        shadowDepthTexture.setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        shadowDepthTexture.setParameter(GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        shadowDepthTexture.setParameter(GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowDepthTexture.getHandle(), 0); MOGL_ASSERT_GLSTATE();
        glDrawBuffer(GL_NONE); MOGL_ASSERT_GLSTATE();
        if (!mogl::FrameBuffer::isComplete(mogl::FrameBuffer::Target::FrameBuffer))
            throw (std::runtime_error("bad framebuffer"));
    }
    catch (const std::runtime_error& e) {
        throw (std::runtime_error(std::string("Shadow map framebuffer error: ") + e.what()));
    }

    // Frame buffer for the normals / normalized depth
    try {
        screenDepthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        glActiveTexture(GL_TEXTURE0); MOGL_ASSERT_GLSTATE();
        screenDepthNormalTexture.bind();
        screenDepthNormalTexture.setImage2D(0, static_cast<GLint>(GL_RGB), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        screenDepthNormalTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        mogl::FrameBuffer::setTexture2D(mogl::FrameBuffer::Target::FrameBuffer, mogl::FrameBuffer::Attachment::Color0, screenDepthNormalTexture);
        screenDepthTexture.bind();
        screenDepthTexture.setImage2D(0, static_cast<GLint>(GL_DEPTH_COMPONENT16), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        screenDepthTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        screenDepthTexture.setParameter(GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        screenDepthTexture.setParameter(GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, screenDepthTexture.getHandle(), 0);

        GLenum DrawBuffers[] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, DrawBuffers); MOGL_ASSERT_GLSTATE();
        if (!mogl::FrameBuffer::isComplete(mogl::FrameBuffer::Target::FrameBuffer))
            throw (std::runtime_error("bad framebuffer"));
    }
    catch (const std::runtime_error& e) {
        throw (std::runtime_error(std::string("SSAO framebuffer error: ") + e.what()));
    }

    vao.bind();
    quadBuffer.bind();
    quadBuffer.setData(quad->getVertexBufferSize(), quad->getVertexBuffer(), GL_STATIC_DRAW);
    teapotVBuffer.bind();
    teapotVBuffer.setData(model->getVertexBufferSize(), model->getVertexBuffer(), GL_STATIC_DRAW);
    teapotNBuffer.bind();
    teapotNBuffer.setData(model->getNormalBufferSize(), model->getNormalBuffer(), GL_STATIC_DRAW);

    cam.setRotation(M_PI_2, -0.3f);
    cam.update(Camera::Spherical);
    Spline s(3);
    s.addControlPoint(glm::vec3(10, 2, 0));
    s.addControlPoint(glm::vec3(0, 7, 10));
    s.addControlPoint(glm::vec3(-10, 2, 0));
    s.addControlPoint(glm::vec3(0, 7, -10));
    s.addControlPoint(glm::vec3(10, 2, 0));
    s.build();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glClearColor(0.2f, 0.5f, 0.0f, 0.0f);

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

        float   t = fmod(frameTime, 35.0f) / 35.0f;
        auto    ip_square = [] (float time) { return time * time; };
        auto    ip_sin = [] (float time) { return 0.5 * sin(time * M_PI * 2.0f) + 0.5; };
        auto    ip_reverse = [] (float time) { return 1.0f - time; };

        translation = s.eval(ip_square(ip_reverse(ip_sin(t))));

        float               nearPlane   = 0.1f;
        float               farPlane    = 100.0f;
        glm::mat4           Projection  = glm::perspective(45.0f, static_cast<float>(ctx.getWindowSize().x) / static_cast<float>(ctx.getWindowSize().y), nearPlane, farPlane);
//         glm::mat4           View        = cam.getViewMatrix();
        glm::mat4           View        = glm::lookAt(translation, glm::vec3(), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4           Model       = glm::translate(glm::scale(glm::mat4(1.0), glm::vec3(3.0)), glm::vec3(0.0, 12.0, 0.0));
        glm::mat4           MV          = View * Model;
        glm::mat4           MVP         = Projection * MV;
        glm::vec3           lightInvDir = glm::vec3(0.5f,2,2);
        glm::mat4           depthProjectionMatrix = glm::ortho<float>(-10,10,-10,10,-10,20);
        glm::mat4           depthViewMatrix = glm::lookAt(lightInvDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4           depthModelMatrix = Model;
        glm::mat4           depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
        glm::mat4           depthBiasMVP = biasMatrix * depthMVP;

        timeQuery.begin();

        // Shadow depth buffer pass
        shadowDepthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        glViewport(0, 0, 1024, 1024);
        glCullFace(GL_BACK);
        glClear(GL_DEPTH_BUFFER_BIT);

        depthpassshader.use();
        depthpassshader.setUniformMatrixPtr<4>("MVP", &depthMVP[0][0]);

        teapotVBuffer.bind();
        depthpassshader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);

        glEnableVertexAttribArray(0); MOGL_ASSERT_GLSTATE();
        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3); MOGL_ASSERT_GLSTATE();
        glDisableVertexAttribArray(0); MOGL_ASSERT_GLSTATE();

        // Fullscreen depth buffer

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
//         screenDepthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        normdepthpassshader.use();
        normdepthpassshader.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);
        normdepthpassshader.setUniformMatrixPtr<4>("M", &Model[0][0]);
        normdepthpassshader.setUniform<GLfloat>("farPlane", farPlane);
        normdepthpassshader.setUniform<GLfloat>("nearPlane", nearPlane);

        teapotVBuffer.bind();
        normdepthpassshader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);
        teapotNBuffer.bind();
        normdepthpassshader.setVertexAttribPointer("vertexNormal_modelspace", 3, GL_FLOAT);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);

        //Render shading with shadow map + ssao
        postFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);
        shader.setUniformMatrixPtr<4>("DepthBiasMVP", &depthBiasMVP[0][0]);
        shader.setUniformMatrixPtr<4>("MV", &MV[0][0]);
        shader.setUniformMatrixPtr<4>("V", &View[0][0]);
        shader.setUniformMatrixPtr<4>("M", &Model[0][0]);
        shader.setUniformPtr<3>("lightInvDirection_worldspace", &lightInvDir[0]);
        shader.setUniform<GLfloat>("frameBufSize", ctx.getWindowSize().x, ctx.getWindowSize().y);

        glActiveTexture(GL_TEXTURE0);
        shadowDepthTexture.bind();
        shader.setUniform<GLint>("shadowMap", 0);

        glActiveTexture(GL_TEXTURE1);
        screenDepthTexture.bind();
        shader.setUniform<GLint>("depthBuffer", 1);

        teapotVBuffer.bind();
        shader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);
        teapotNBuffer.bind();
        shader.setVertexAttribPointer("vertexNormal_modelspace", 3, GL_FLOAT);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);

        //Apply post-process effects
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        postshader.use();

        glActiveTexture(GL_TEXTURE0);
        frameTexture.bind();

        postshader.setUniform<GLint>("renderedTexture", 0);
        postshader.setUniform<GLfloat>("frameBufSize", ctx.getWindowSize().x, ctx.getWindowSize().y);
//         postshader.setUniformSubroutine(mogl::ShaderObject::ShaderType::FragmentShader, "tonemapSelector", "notonemap");
//         if (controller.isHeld(SixAxis::Buttons::Circle))
//             postshader.setUniformSubroutine(mogl::ShaderObject::ShaderType::FragmentShader, "tonemapSelector", "uncharted2tonemap");
//         else if (controller.isHeld(SixAxis::Buttons::Square))
//             postshader.setUniformSubroutine(mogl::ShaderObject::ShaderType::FragmentShader, "tonemapSelector", "reinhardtonemap");

        quadBuffer.bind();
        postshader.setVertexAttribPointer("v_coord", 3, GL_FLOAT);

        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLES, 0, quad->getTriangleCount() * 3);
        glDisableVertexAttribArray(0);

        timeQuery.end();
        double glms = static_cast<double>(timeQuery.getResult<GLuint>(GL_QUERY_RESULT)) * 0.000001;

        ctx.swapBuffers();
        frameTime = ctx.getTime() - frameTime;
        ctx.setTitle("Full frame " + std::to_string(frameTime * 1000.0f) + " ms / gpu frame " + std::to_string(glms) + " ms " + std::to_string(ctx.getWindowSize().x) + "x"  + std::to_string(ctx.getWindowSize().y));
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
    Model*                      quad = loader.load("rc/model/quad.obj");
    mogl::QueryObject           timeQuery(GL_TIME_ELAPSED);
    glm::mat4                   biasMatrix(
                                    0.5, 0.0, 0.0, 0.0,
                                    0.0, 0.5, 0.0, 0.0,
                                    0.0, 0.0, 0.5, 0.0,
                                    0.5, 0.5, 0.5, 1.0
                                );

    model->debugDump();
    ShaderHelper::loadSimpleShader(depthpassshader, "rc/shader/depth_pass.vert", "rc/shader/depth_pass.frag");
    ShaderHelper::loadSimpleShader(shader, "rc/shader/shadow.vert", "rc/shader/shadow.frag");
    ShaderHelper::loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/fxaa.frag");

    depthpassshader.printDebug();
    shader.printDebug();
    postshader.printDebug();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glClearColor(0.2f, 0.5f, 0.0f, 0.0f);

    quadBuffer.bind();
    quadBuffer.setData(quad->getVertexBufferSize(), quad->getVertexBuffer(), GL_STATIC_DRAW);

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

        depthpassshader.use();
        depthpassshader.setUniformMatrixPtr<4>("MVP", &depthMVP[0][0]);

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

        shader.use();
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

        postshader.use();

        glActiveTexture(GL_TEXTURE0);
        frameTexture.bind();

        postshader.setUniform<GLint>("renderedTexture", 0);
        postshader.setUniform<GLfloat>("frameBufSize", ctx.getWindowSize().x, ctx.getWindowSize().y);

        glEnableVertexAttribArray(0);
        quadBuffer.bind();
        postshader.setVertexAttribPointer("v_coord", 3, GL_FLOAT);

        glDrawArrays(GL_TRIANGLES, 0, quad->getTriangleCount() * 3);
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
