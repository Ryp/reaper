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
    Model*                      model = loader.load("rc/model/suzanne.obj");
    mogl::QueryObject           timeQuery(GL_TIME_ELAPSED);
    glm::mat4                   biasMatrix(
                                    0.5, 0.0, 0.0, 0.0,
                                    0.0, 0.5, 0.0, 0.0,
                                    0.0, 0.0, 0.5, 0.0,
                                    0.5, 0.5, 0.5, 1.0
                                );

    loadSimpleShader(depthpassshader, "rc/shader/depth_pass.vert", "rc/shader/depth_pass.frag");
    loadSimpleShader(shader, "rc/shader/shadow.vert", "rc/shader/shadow.frag");
    loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/post.frag");

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
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGB), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
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
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_DEPTH_COMPONENT16), 1024, 1024, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0);
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

    while (ctx.isOpen())
    {
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
        glm::mat4           Model       = glm::mat4(1.0f);
        glm::mat4           MV          = View * Model;
        glm::mat4           MVP         = Projection * MV;
        glm::vec3           lightInvDir = glm::vec3(0.5f,2,2);
        glm::mat4           depthProjectionMatrix = glm::ortho<float>(-10,10,-10,10,-10,20);
        glm::mat4           depthViewMatrix = glm::lookAt(lightInvDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4           depthModelMatrix = glm::mat4(1.0);
        glm::mat4           depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
        glm::mat4           depthBiasMVP = biasMatrix * depthMVP;

        frameTime = ctx.getTime();
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
        postshader.setUniform<GLfloat>("time", ctx.getTime());

        glEnableVertexAttribArray(0);
        quadBuffer.bind();
        postshader.setVertexAttribPointer("v_coord", 3, GL_FLOAT);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);

        timeQuery.end();
        double glms = static_cast<double>(timeQuery.getResult<GLuint>(GL_QUERY_RESULT)) * 0.0000001;

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
        pcf_test(ctx, controller);
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
