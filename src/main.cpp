#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace gl;

#include "glcontext.hpp"

#include <mogl/exception/moglexception.hpp>
#include <mogl/shader/shaderprogram.hpp>
#include <mogl/texture/texture.hpp>
#include <mogl/framebuffer/framebuffer.hpp>
#include <mogl/renderbuffer/renderbuffer.hpp>
#include <mogl/buffer/buffer.hpp>
#include <mogl/buffer/vertexarray.hpp>
#include <mogl/sync/query.hpp>
#include <mogl/states/states.hpp>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/compatibility.hpp>

#include "input/SixAxis.hpp"
#include "Camera.hpp"
#include "math/spline.hpp"
#include "resource/shaderhelper.hpp"
#include "resource/imageloader.hpp"
#include "resource/resourcemanager.hpp"
#include "pipeline/mesh.hpp"

void    debugCallback(GLenum /*source*/, GLenum type, GLuint id, GLenum severity,
                      GLsizei /*length*/, const GLchar *message, const void */*userParam*/)
{
    std::cout << "DEBUG message: "<< message << std::endl;
    std::cout << "type: ";
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            std::cout << "ERROR";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            std::cout << "DEPRECATED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            std::cout << "UNDEFINED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            std::cout << "PORTABILITY";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            std::cout << "PERFORMANCE";
            break;
        case GL_DEBUG_TYPE_OTHER:
            std::cout << "OTHER";
            break;
        default:
            break;
    }
    std::cout << " id: " << id << std::cout << " severity: ";
    switch (severity){
        case GL_DEBUG_SEVERITY_LOW:
            std::cout << "LOW";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            std::cout << "MEDIUM";
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            std::cout << "HIGH";
            break;
        default:
            break;
    }
    std::cout << std::endl;
}

void    hdr_test(GLContext& ctx, SixAxis& controller)
{
    ResourceManager     resourceMgr("rc");
    float               frameTime;
    Camera              cam(glm::vec3(-5.0, 3.0, 0.0));
    mogl::VertexArray   vao;
    Model*              quad = resourceMgr.getModel("model/quad.obj");
    Mesh                mesh(resourceMgr.getModel("model/ship.obj"));
    mogl::ShaderProgram shader;
    mogl::ShaderProgram postshader;
    mogl::ShaderProgram normdepthpassshader;
    mogl::ShaderProgram depthpassshader;
    mogl::Buffer        quadBuffer(GL_ARRAY_BUFFER);
    mogl::FrameBuffer   postFrameBuffer;
    mogl::FrameBuffer   shadowDepthFrameBuffer;
    mogl::FrameBuffer   screenDepthFrameBuffer;
    mogl::Texture       frameTexture(GL_TEXTURE_2D);
    mogl::Texture       shadowTexture(GL_TEXTURE_2D);
    mogl::Texture       screenDepthTexture(GL_TEXTURE_2D);
    mogl::Texture       screenNormalTexture(GL_TEXTURE_2D);
    mogl::Texture       noiseTexture(GL_TEXTURE_2D);
    mogl::RenderBuffer  postDepthBuffer;
    mogl::Query         timeQuery(GL_TIME_ELAPSED);
    mogl::Query         polyQuery(GL_PRIMITIVES_GENERATED);
    mogl::Query         samplesQuery(GL_SAMPLES_PASSED);
    float               shadowMapResolution = 4096;
    glm::vec3           translation;
    glm::mat4           biasMatrix(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 0.5, 0.0,
        0.5, 0.5, 0.5, 1.0
    );

    ShaderHelper::loadSimpleShader(depthpassshader, "rc/shader/depth_pass.vert", "rc/shader/depth_pass.frag");
    ShaderHelper::loadSimpleShader(normdepthpassshader, "rc/shader/depth_pass_normalized.vert", "rc/shader/depth_pass_normalized.frag");
    ShaderHelper::loadSimpleShader(shader, "rc/shader/shadow_ssao.vert", "rc/shader/shadow_ssao.frag");
    ShaderHelper::loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/fxaa.frag");
//     loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/tonemapping.frag");

    shader.printDebug();

    // Frame buffer for post processing
    try {
        frameTexture.setImage2D(0, static_cast<GLint>(GL_RGB16F), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_RGB, GL_HALF_FLOAT, 0);
        frameTexture.set(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        frameTexture.set(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        frameTexture.set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        frameTexture.set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        postDepthBuffer.setStorage(GL_DEPTH_COMPONENT, ctx.getWindowSize().x, ctx.getWindowSize().y);

        frameTexture.bind(0);
        postFrameBuffer.setTexture(GL_COLOR_ATTACHMENT0, frameTexture);
        postFrameBuffer.setRenderBuffer(GL_DEPTH_ATTACHMENT, postDepthBuffer);
        postFrameBuffer.setDrawBuffer(GL_COLOR_ATTACHMENT0);
        if (!postFrameBuffer.isComplete(GL_FRAMEBUFFER))
            throw (std::runtime_error("bad framebuffer"));
    }
    catch (const std::runtime_error& e) {
        throw (std::runtime_error(std::string("Main framebuffer error: ") + e.what()));
    }

    // Frame buffer for the shadow map
    try {
        GLfloat   border[] = {1.0f, 0.0f, 0.0f, 0.0f};
        shadowTexture.setImage2D(0, static_cast<GLint>(GL_DEPTH_COMPONENT), shadowMapResolution, shadowMapResolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
        shadowTexture.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        shadowTexture.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        shadowTexture.set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        shadowTexture.set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        shadowTexture.set(GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        shadowTexture.set(GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        shadowTexture.set(GL_TEXTURE_BORDER_COLOR, border);

        shadowTexture.bind(0);
        shadowDepthFrameBuffer.setTexture(GL_DEPTH_ATTACHMENT, shadowTexture);
        shadowDepthFrameBuffer.setDrawBuffer(GL_NONE);
        if (!shadowDepthFrameBuffer.isComplete(GL_FRAMEBUFFER))
            throw (std::runtime_error("bad framebuffer"));
    }
    catch (const std::runtime_error& e) {
        throw (std::runtime_error(std::string("Shadow map framebuffer error: ") + e.what()));
    }

    // Frame buffer for the normals / depth
    try {
        screenNormalTexture.setImage2D(0, static_cast<GLint>(GL_RGB), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        screenNormalTexture.set(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        screenNormalTexture.set(GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        screenDepthTexture.setImage2D(0, static_cast<GLint>(GL_DEPTH_COMPONENT24), ctx.getWindowSize().x, ctx.getWindowSize().y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        screenDepthTexture.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        screenDepthTexture.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        screenDepthTexture.set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        screenDepthTexture.set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        screenDepthTexture.set(GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        screenDepthTexture.set(GL_TEXTURE_COMPARE_FUNC, GL_LESS);

        screenNormalTexture.bind(0);
        screenDepthFrameBuffer.setTexture(GL_COLOR_ATTACHMENT0, screenNormalTexture);
        screenDepthTexture.bind(1);
        screenDepthFrameBuffer.setTexture(GL_DEPTH_ATTACHMENT, screenDepthTexture);
        screenDepthFrameBuffer.setDrawBuffer(GL_COLOR_ATTACHMENT0);
        if (!screenDepthFrameBuffer.isComplete(GL_FRAMEBUFFER))
            throw (std::runtime_error("bad framebuffer"));
    }
    catch (const std::runtime_error& e) {
        throw (std::runtime_error(std::string("SSAO framebuffer error: ") + e.what()));
    }

    vao.bind();

    mesh.init();

    quadBuffer.bind();
    quadBuffer.setData(quad->getVertexBufferSize(), quad->getVertexBuffer(), GL_STATIC_DRAW);

    noiseTexture.bind(0);
    ImageLoader::loadDDS("rc/texture/ship.dds", noiseTexture);

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
//         glm::mat4   Model       = glm::translate(glm::scale(glm::mat4(1.0), glm::vec3(3.0)), glm::vec3(0.0, 12.0, 0.0));
        glm::mat4   Model       = glm::mat4(1.0);
//         glm::mat4   Model       = glm::scale(glm::mat4(1.0), glm::vec3(0.1));
        glm::mat4   MV          = View * Model;
        glm::mat4   MVP         = Projection * MV;
        glm::vec3   lightInvDir = glm::vec3(0.5f,2,2);
        glm::mat4   depthProjectionMatrix = glm::ortho<float>(-80,80,-80,80,-100,200);
        glm::mat4   depthViewMatrix = glm::lookAt(lightInvDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4   depthModelMatrix = Model;
        glm::mat4   depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
        glm::mat4   depthBiasMVP = biasMatrix * depthMVP;

        timeQuery.begin();
        polyQuery.begin();
        samplesQuery.begin();

        // Shadow depth buffer pass
        shadowDepthFrameBuffer.bind(GL_FRAMEBUFFER);
        mogl::setViewport(0, 0, shadowMapResolution, shadowMapResolution);
        mogl::setCullFace(GL_BACK);
        mogl::enable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(5.0f, 4.0f);
        glClear(GL_DEPTH_BUFFER_BIT);
        depthpassshader.use();
        depthpassshader.setUniformMatrixPtr<4>("MVP", &depthMVP[0][0]);
        mesh.draw(depthpassshader, Mesh::Vertex);

        // Fullscreen depth buffer
        screenDepthFrameBuffer.bind(GL_FRAMEBUFFER);
        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        mogl::setCullFace(GL_BACK);
        mogl::disable(GL_POLYGON_OFFSET_FILL);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        normdepthpassshader.use();
        normdepthpassshader.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);
        mesh.draw(normdepthpassshader, Mesh::Vertex | Mesh::Normal);

        //Render shading with shadow map + ssao
        postFrameBuffer.bind(GL_FRAMEBUFFER);
        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        mogl::setCullFace(GL_BACK);

        GLfloat clearDepth[] = { 1.0f };
        postFrameBuffer.clear(GL_COLOR, 0, &glm::vec4(1.0f, 1.0f, 0.0f, 0.0f)[0]);
        postFrameBuffer.clear(GL_DEPTH, 0, clearDepth);

        shader.use();
        shader.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);
        shader.setUniformMatrixPtr<4>("DepthBiasMVP", &depthBiasMVP[0][0]);
//         shader.setUniformMatrixPtr<4>("DepthBias", &biasMatrix[0][0]);
        shader.setUniformMatrixPtr<4>("MV", &MV[0][0]);
        shader.setUniformMatrixPtr<4>("V", &View[0][0]);
        shader.setUniformMatrixPtr<4>("M", &Model[0][0]);
        shader.setUniformPtr<3>("lightInvDirection_worldspace", &lightInvDir[0]);

        shadowTexture.bind(0);
        shader.setUniform<GLint>("shadowMapTex", 0);
        screenDepthTexture.bind(1);
        shader.setUniform<GLint>("depthBufferTex", 1);
        noiseTexture.bind(2);
        shader.setUniform<GLint>("noiseTex", 2);

        mesh.draw(shader, Mesh::Vertex | Mesh::Normal | Mesh::UV);

        //Apply post-process effects
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        postshader.use();

        frameTexture.bind(0);
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
        polyQuery.end();
        samplesQuery.end();

        double glms = static_cast<double>(timeQuery.get<GLuint>(GL_QUERY_RESULT)) * 0.000001;
        unsigned int poly = polyQuery.get<GLuint>(GL_QUERY_RESULT);
        unsigned int samples = samplesQuery.get<GLuint>(GL_QUERY_RESULT);

        ctx.swapBuffers();
        frameTime = ctx.getTime() - frameTime;
        ctx.setTitle(std::to_string(samples) + " Samples " + std::to_string(poly) + " Primitives | Total " + std::to_string(frameTime * 1000.0f) + " ms / GPU " + std::to_string(glms) + " ms " + std::to_string(ctx.getWindowSize().x) + "x"  + std::to_string(ctx.getWindowSize().y));
    }
    delete kernel;
}

int main(int /*ac*/, char** /*av*/)
{
    try {
        GLContext   ctx;
        SixAxis     controller("/dev/input/js0");

        ctx.create(1600, 900, 4, 5, false, true);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, nullptr);
        GLuint unusedIds = 0;
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &unusedIds, GL_TRUE);
        hdr_test(ctx, controller);
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
