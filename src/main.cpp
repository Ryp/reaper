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
#include <mogl/states/states.hpp>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/compatibility.hpp>

#include "ModelLoader.hh"
#include "SixAxis.hpp"
#include "Camera.hpp"
#include "spline.hpp"
#include "shaderhelper.hpp"
#include "imageloader.hpp"

void    hdr_test(GLContext& ctx, SixAxis& controller)
{
    float                       frameTime;
    Camera                      cam(glm::vec3(-5.0, 3.0, 0.0));
    ModelLoader                 loader;
    Model*                      model = loader.load("rc/model/sibenik.obj");
    Model*                      quad = loader.load("rc/model/quad.obj");
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
    mogl::TextureObject         screenNormalTexture(GL_TEXTURE_2D);
    mogl::TextureObject         noiseTexture(GL_TEXTURE_2D);
    mogl::RenderBufferObject    postDepthBuffer;
    mogl::QueryObject           timeQuery(GL_TIME_ELAPSED);
    float                       shadowMapResolution = 4096;
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

    shader.printDebug();

    // Frame buffer for post processing
    try {
        postFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        mogl::setActiveTexture(GL_TEXTURE0);
        frameTexture.bind();
        frameTexture.setImage2D(0, static_cast<GLint>(GL_RGB16F), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_RGB, GL_HALF_FLOAT, 0);
        frameTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        frameTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        frameTexture.setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        frameTexture.setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        mogl::FrameBuffer::setTexture2D(mogl::FrameBuffer::Target::FrameBuffer, mogl::FrameBuffer::Attachment::Color0, frameTexture);
        postDepthBuffer.bind();
        postDepthBuffer.setStorage(GL_DEPTH_COMPONENT, ctx.getWindowSize().x, ctx.getWindowSize().y);
        mogl::FrameBuffer::setRenderBuffer(mogl::FrameBuffer::Target::FrameBuffer, mogl::FrameBuffer::Attachment::Depth, postDepthBuffer);

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
        GLfloat   border[] = {1.0f, 0.0f, 0.0f, 0.0f};
        shadowDepthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        mogl::setActiveTexture(GL_TEXTURE0);
        shadowDepthTexture.bind();
        shadowDepthTexture.setImage2D(0, static_cast<GLint>(GL_DEPTH_COMPONENT), shadowMapResolution, shadowMapResolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
        shadowDepthTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        shadowDepthTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        shadowDepthTexture.setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        shadowDepthTexture.setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        shadowDepthTexture.setParameter(GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        shadowDepthTexture.setParameter(GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        shadowDepthTexture.setParameterPtr(GL_TEXTURE_BORDER_COLOR, border);
        mogl::FrameBuffer::setTexture(mogl::FrameBuffer::Target::FrameBuffer, mogl::FrameBuffer::Attachment::Depth, shadowDepthTexture);

        glDrawBuffer(GL_NONE); MOGL_ASSERT_GLSTATE();
        if (!mogl::FrameBuffer::isComplete(mogl::FrameBuffer::Target::FrameBuffer))
            throw (std::runtime_error("bad framebuffer"));
    }
    catch (const std::runtime_error& e) {
        throw (std::runtime_error(std::string("Shadow map framebuffer error: ") + e.what()));
    }

    // Frame buffer for the normals / depth
    try {
        screenDepthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        mogl::setActiveTexture(GL_TEXTURE0);
        screenNormalTexture.bind();
        screenNormalTexture.setImage2D(0, static_cast<GLint>(GL_RGB), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        screenNormalTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        screenNormalTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        mogl::FrameBuffer::setTexture2D(mogl::FrameBuffer::Target::FrameBuffer, mogl::FrameBuffer::Attachment::Color0, screenNormalTexture);
        mogl::setActiveTexture(GL_TEXTURE1);
        screenDepthTexture.bind();
        screenDepthTexture.setImage2D(0, static_cast<GLint>(GL_DEPTH_COMPONENT24), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        screenDepthTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        screenDepthTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        screenDepthTexture.setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        screenDepthTexture.setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        screenDepthTexture.setParameter(GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        screenDepthTexture.setParameter(GL_TEXTURE_COMPARE_FUNC, GL_LESS);
        mogl::FrameBuffer::setTexture2D(mogl::FrameBuffer::Target::FrameBuffer, mogl::FrameBuffer::Attachment::Depth, screenDepthTexture);

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

    mogl::setActiveTexture(GL_TEXTURE0);
    noiseTexture.bind();
    ImageLoader::loadDDS("rc/texture/noise.dds", noiseTexture);

    int ssaoKernelSize = 32;
    glm::fvec3* kernel = new(std::nothrow) glm::fvec3[ssaoKernelSize];
    for (int i = 0; i < ssaoKernelSize; ++i) {
        kernel[i] = glm::fvec3(
            glm::linearRand(-1.0f, 1.0f),
                               glm::linearRand(-1.0f, 1.0f),
                               glm::linearRand(0.0f, 1.0f)
        );
        kernel[i] = glm::normalize(kernel[i]);
        float scale = (float)i / (float)ssaoKernelSize;
        kernel[i] *= glm::lerp(0.1f, 1.0f, scale * scale);
    }

    mogl::enable(GL_DEPTH_TEST);
    mogl::enable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
    glClearColor(0.5f, 0.0f, 0.5f, 0.0f);

    while (ctx.isOpen())
    {
        frameTime = ctx.getTime();

        controller.update();
        if (controller.isHeld(SixAxis::Buttons::Start))
            break;
        float speedup = 1.0 + (controller.getAxis(SixAxis::Axes::L2Pressure) * 0.5 + 0.5) * 10.0f;
        cam.move(controller.getAxis(SixAxis::LeftAnalogY) * speedup / -10.0f, controller.getAxis(SixAxis::LeftAnalogX) * speedup / -10.0f, 0.0f);
        cam.rotate(controller.getAxis(SixAxis::RightAnalogX) / 10.0f, controller.getAxis(SixAxis::RightAnalogY) / -10.0f);
        cam.update(Camera::Spherical);
        if (controller.isPressed(SixAxis::Buttons::Select))
            cam.reset();

        float       nearPlane   = 0.1f;
        float       farPlane    = 1000.0f;
        glm::mat4   Projection  = glm::perspective(45.0f, static_cast<float>(ctx.getWindowSize().x) / static_cast<float>(ctx.getWindowSize().y), nearPlane, farPlane);
        glm::mat4   View        = cam.getViewMatrix();
        glm::mat4   Model       = glm::translate(glm::scale(glm::mat4(1.0), glm::vec3(3.0)), glm::vec3(0.0, 12.0, 0.0));
//         glm::mat4   Model       = glm::mat4(1.0);
        glm::mat4   MV          = View * Model;
        glm::mat4   MVP         = Projection * MV;
        glm::vec3   lightInvDir = glm::vec3(0.5f,2,2);
        glm::mat4   depthProjectionMatrix = glm::ortho<float>(-80,80,-80,80,-100,200);
        glm::mat4   depthViewMatrix = glm::lookAt(lightInvDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4   depthModelMatrix = Model;
        glm::mat4   depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
        glm::mat4   depthBiasMVP = biasMatrix * depthMVP;

        timeQuery.begin();

        // Shadow depth buffer pass
        shadowDepthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        mogl::setViewport(0, 0, shadowMapResolution, shadowMapResolution);
        mogl::setCullFace(GL_BACK);
        mogl::enable(GL_POLYGON_OFFSET_FILL);
        float amount = controller.getAxis(SixAxis::R2Pressure) * (controller.getAxis(SixAxis::Axes::L2Pressure) * 0.5 + 0.5) * 10.0f;
        glPolygonOffset(5.0f, 4.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        depthpassshader.use();
        depthpassshader.setUniformMatrixPtr<4>("MVP", &depthMVP[0][0]);

        teapotVBuffer.bind();
        depthpassshader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);

        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3);
        glDisableVertexAttribArray(0);

        // Fullscreen depth buffer
        screenDepthFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        mogl::setCullFace(GL_BACK);
        mogl::disable(GL_POLYGON_OFFSET_FILL);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        normdepthpassshader.use();
        normdepthpassshader.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);

        teapotVBuffer.bind();
        normdepthpassshader.setVertexAttribPointer("vertexPosition_modelspace", 3, GL_FLOAT);
        teapotNBuffer.bind();
        normdepthpassshader.setVertexAttribPointer("vertexNormal_modelspace", 3, GL_FLOAT);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glDrawArrays(GL_TRIANGLES, 0, model->getTriangleCount() * 3);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);

        //Render shading with shadow map + ssao
        postFrameBuffer.bind(mogl::FrameBuffer::Target::FrameBuffer);
        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        mogl::setCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);
        shader.setUniformMatrixPtr<4>("DepthBiasMVP", &depthBiasMVP[0][0]);
        shader.setUniformMatrixPtr<4>("DepthBias", &biasMatrix[0][0]);
        shader.setUniformMatrixPtr<4>("MV", &MV[0][0]);
        shader.setUniformMatrixPtr<4>("V", &View[0][0]);
        shader.setUniformMatrixPtr<4>("M", &Model[0][0]);
        shader.setUniformPtr<3>("lightInvDirection_worldspace", &lightInvDir[0]);

        mogl::setActiveTexture(GL_TEXTURE0);
        shadowDepthTexture.bind();
        shader.setUniform<GLint>("shadowMapTex", 0);

        mogl::setActiveTexture(GL_TEXTURE1);
        screenDepthTexture.bind();
        shader.setUniform<GLint>("depthBufferTex", 1);

        mogl::setActiveTexture(GL_TEXTURE2);
        noiseTexture.bind();
//         shader.setUniform<GLint>("noiseTex", 2);

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
        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        postshader.use();

        mogl::setActiveTexture(GL_TEXTURE0);
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
        ctx.setTitle("A=" + std::to_string(amount) + "Full frame " + std::to_string(frameTime * 1000.0f) + " ms / gpu frame " + std::to_string(glms) + " ms " + std::to_string(ctx.getWindowSize().x) + "x"  + std::to_string(ctx.getWindowSize().y));
    }
    delete kernel;
}

int main(int /*ac*/, char** /*av*/)
{
    try {
        GLContext   ctx;
        SixAxis     controller("/dev/input/js0");

        ctx.create(1600, 900, false);
        ctx.printLog(false);
        hdr_test(ctx, controller);
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
