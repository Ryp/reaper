#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace gl;

#include "glcontext.hpp"

#include <mogl/exception/moglexception.hpp>
#include <mogl/object/shader/shaderprogram.hpp>
#include <mogl/object/texture.hpp>
#include <mogl/object/framebuffer.hpp>
#include <mogl/object/renderbuffer.hpp>
#include <mogl/object/buffer/arraybuffer.hpp>
#include <mogl/object/buffer/elementarraybuffer.hpp>
#include <mogl/object/buffer/uniformbuffer.hpp>
#include <mogl/object/vertexarray.hpp>
#include <mogl/object/query.hpp>
#include <mogl/function/states.hpp>

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

#include "unixfilewatcher.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw_gl3.h"

void    hdr_test(GLContext& ctx, SixAxis& controller)
{
    ResourceManager     resourceMgr("rc");
    float               frameTime;
    Camera              cam(glm::vec3(-5.0, 3.0, 0.0));
    mogl::VAO           vao;
    Model*              quad = resourceMgr.getModel("model/quad.obj");
    Mesh                mesh(resourceMgr.getModel("model/sphere.obj"));
    mogl::ShaderProgram shader;
    mogl::ShaderProgram postshader;
    mogl::ShaderProgram normdepthpassshader;
    mogl::ShaderProgram depthpassshader;
    mogl::VBO           quadBuffer;
    mogl::FrameBuffer   postFrameBuffer;
    mogl::FrameBuffer   shadowDepthFrameBuffer;
    mogl::FrameBuffer   screenDepthFrameBuffer;
    mogl::Texture       frameTexture(GL_TEXTURE_2D);
    mogl::Texture       shadowTexture(GL_TEXTURE_2D);
    mogl::Texture       screenDepthTexture(GL_TEXTURE_2D);
    mogl::Texture       screenNormalTexture(GL_TEXTURE_2D);
    mogl::Texture       modelTexture(GL_TEXTURE_2D);
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
    ShaderHelper::loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/tonemapping.frag");
//     loadSimpleShader(postshader, "rc/shader/post/passthrough.vert", "rc/shader/post/tonemapping.frag");

    shader.printDebug();

    // Frame buffer for post processing
    frameTexture.setStorage2D(1, GL_RGB16F, ctx.getWindowSize().x, ctx.getWindowSize().y);
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

    // Frame buffer for the shadow map
    GLfloat   border[] = {1.0f, 0.0f, 0.0f, 0.0f};
    shadowTexture.setStorage2D(1, GL_DEPTH_COMPONENT32, shadowMapResolution, shadowMapResolution);
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

    // Frame buffer for the normals / depth
    screenNormalTexture.setStorage2D(1, GL_RGB8UI, ctx.getWindowSize().x, ctx.getWindowSize().y);
    screenNormalTexture.set(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    screenNormalTexture.set(GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    screenDepthTexture.setStorage2D(1, GL_DEPTH_COMPONENT24, ctx.getWindowSize().x, ctx.getWindowSize().y);
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

    vao.bind();

    mesh.init();

    quadBuffer.bind();
    quadBuffer.setData(quad->getVertexBufferSize(), quad->getVertexBuffer(), GL_STATIC_DRAW);

    ImageLoader::loadDDS("rc/texture/default.dds", modelTexture);

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
        postFrameBuffer.clear(GL_COLOR, 0, &glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)[0]);
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
        modelTexture.bind(2);
        shader.setUniform<GLint>("noiseTex", 2);

        mesh.draw(shader, Mesh::Vertex | Mesh::Normal | Mesh::UV);

        //Apply post-process effects
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        postshader.use();

        frameTexture.bind(0);
        postshader.setUniform<GLint>("renderedTexture", 0);
//         postshader.setUniform<GLfloat>("frameBufSize", ctx.getWindowSize().x, ctx.getWindowSize().y);
        if (controller.isHeld(SixAxis::Buttons::Circle))
            postshader.setUniformSubroutine(GL_FRAGMENT_SHADER, "tonemapSelector", "uncharted2tonemap");
        else if (controller.isHeld(SixAxis::Buttons::Square))
            postshader.setUniformSubroutine(GL_FRAGMENT_SHADER, "tonemapSelector", "reinhardtonemap");
        else
            postshader.setUniformSubroutine(GL_FRAGMENT_SHADER, "tonemapSelector", "notonemap");

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

void    bloom_test(GLContext& ctx, SixAxis& controller)
{
    UnixFileWatcher     ufw;
    mogl::FrameBuffer   render_fbo;
    mogl::FrameBuffer   filter_fbo[2];
    mogl::Texture       tex_scene(GL_TEXTURE_2D);
    mogl::Texture       tex_brightpass(GL_TEXTURE_2D);
    mogl::Texture       tex_depth(GL_TEXTURE_2D);
    mogl::Texture       tex_lut(GL_TEXTURE_1D);
    mogl::Texture       tex_filter[2] = {GL_TEXTURE_2D, GL_TEXTURE_2D};
    mogl::Texture       sphereTex(GL_TEXTURE_2D);
    mogl::ShaderProgram program_envmap;
    mogl::ShaderProgram program_render;
    mogl::ShaderProgram program_filter;
    mogl::ShaderProgram program_resolve;
    mogl::VAO           vao;
    mogl::UniformBuffer ubo_transform;
    mogl::UniformBuffer ubo_material;
    ResourceManager     resourceMgr("rc");
    Mesh                mesh(resourceMgr.getModel("model/sphere.obj"));
    float       exposure(1.0f);
    bool        paused(false);
    float       bloom_thresh_min(0.8f);
    float       bloom_thresh_max(1.2f);
    const int   sphereCount = 32;

    struct
    {
        struct
        {
            int bloom_thresh_min;
            int bloom_thresh_max;
        } scene;
        struct
        {
            int exposure;
        } resolve;
    } uniforms;

    struct material
    {
        glm::vec3       diffuse_color;
        unsigned int    : 32;
        glm::vec3       specular_color;
        float           specular_power;
        glm::vec3       ambient_color;
        unsigned int    : 32;
    };

    ImageLoader::loadDDS("rc/texture/envmap/lobby.dds", sphereTex);

    static const GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    static const GLfloat exposureLUT[20]   = { 11.0f, 6.0f, 3.2f, 2.8f, 2.2f, 1.90f, 1.80f, 1.80f, 1.70f, 1.70f,  1.60f, 1.60f, 1.50f, 1.50f, 1.40f, 1.40f, 1.30f, 1.20f, 1.10f, 1.00f };

    vao.bind();

    ShaderHelper::loadSimpleShader(program_envmap, "rc/shader/envmap.vert", "rc/shader/envmap.frag");
    ShaderHelper::loadSimpleShader(program_render, "rc/shader/hdrbloom/hdrbloom-scene.vs.glsl", "rc/shader/hdrbloom/hdrbloom-scene.fs.glsl");
    uniforms.scene.bloom_thresh_min = program_render.getUniformLocation("bloom_thresh_min");
    uniforms.scene.bloom_thresh_max = program_render.getUniformLocation("bloom_thresh_max");
    ShaderHelper::loadSimpleShader(program_filter, "rc/shader/hdrbloom/hdrbloom-filter.vs.glsl", "rc/shader/hdrbloom/hdrbloom-filter.fs.glsl");
    ShaderHelper::loadSimpleShader(program_resolve, "rc/shader/hdrbloom/hdrbloom-resolve.vs.glsl", "rc/shader/hdrbloom/hdrbloom-resolve.fs.glsl");
    uniforms.resolve.exposure = program_resolve.getUniformLocation("exposure");

    tex_scene.setStorage2D(1, GL_RGBA16F, ctx.getWindowSize().x, ctx.getWindowSize().y);
    tex_brightpass.setStorage2D(11, GL_RGBA16F, ctx.getWindowSize().x, ctx.getWindowSize().y);
    tex_brightpass.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    tex_brightpass.generateMipmap();
    tex_depth.setStorage2D(1, GL_DEPTH_COMPONENT32F, ctx.getWindowSize().x, ctx.getWindowSize().y);
    render_fbo.setTexture(GL_COLOR_ATTACHMENT0, tex_scene, 0);
    render_fbo.setTexture(GL_COLOR_ATTACHMENT1, tex_brightpass, 0);
    render_fbo.setTexture(GL_DEPTH_ATTACHMENT, tex_depth, 0);
    render_fbo.setDrawBuffers(2, buffers);

    for (int i = 0; i < 2; ++i)
    {
        tex_filter[i].setStorage2D(1, GL_RGBA16F, (i ? ctx.getWindowSize().x : ctx.getWindowSize().y), (i ? ctx.getWindowSize().y : ctx.getWindowSize().x));
        filter_fbo[i].setTexture(GL_COLOR_ATTACHMENT0, tex_filter[i], 0);
        filter_fbo[i].setDrawBuffers(1, buffers);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    tex_lut.setStorage1D(1, GL_R32F, 20);
    tex_lut.setSubImage1D(0, 0, 20, GL_RED, GL_FLOAT, exposureLUT);
    tex_lut.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex_lut.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex_lut.set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

    mesh.init();

    ubo_transform.setData((2 + sphereCount) * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

    ubo_material.setData(sphereCount * sizeof(material), nullptr, GL_STATIC_DRAW);
    material* m = static_cast<material*>(ubo_material.mapRange(0, sphereCount * sizeof(material), static_cast<GLbitfield>(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)));
    float ambient = 0.002f;
    for (int i = 0; i < sphereCount; ++i)
    {
        float fi = 3.14159267f * (float)i / 8.0f;
        m[i].diffuse_color  = glm::vec3(sinf(fi) * 0.5f + 0.5f, sinf(fi + 1.345f) * 0.5f + 0.5f, sinf(fi + 2.567f) * 0.5f + 0.5f);
        m[i].specular_color = glm::vec3(2.8f, 2.8f, 2.9f);
        m[i].specular_power = 30.0f;
        m[i].ambient_color  = glm::vec3(ambient * 0.025f);
        ambient *= 1.5f;
    }
    ubo_material.unmap();

    bool show_test_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImColor(114, 144, 154);

    mogl::enable(GL_CULL_FACE);
    mogl::setCullFace(GL_BACK);
    glDepthFunc(GL_LESS);
    do
    {
        float currentTime = ctx.getTime();
        static float t = 0.0f;
        static const GLfloat black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        static const GLfloat one = 1.0f;

        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplGlfwGL3_NewFrame();

        controller.update();
        if (controller.isPressed(SixAxis::Buttons::Cross))
            paused = !paused;

        if (!paused)
            t = currentTime * 0.05f;

        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        render_fbo.bind(GL_FRAMEBUFFER);
        render_fbo.clear(GL_COLOR, 0, black);
        render_fbo.clear(GL_COLOR, 1, black);
        render_fbo.clear(GL_DEPTH, 0, &one);

        glm::mat4   Projection = glm::perspective(70.0f, (float)ctx.getWindowSize().x / (float)ctx.getWindowSize().y, 1.0f, 100.0f);
        glm::mat4   Model = glm::scale(glm::mat4(1.0f), glm::vec3(50.0f, 50.0f, 50.0f));
        glm::mat4   MP = Projection * Model;

        mogl::disable(GL_DEPTH_TEST);
        program_envmap.use();
        vao.bind();

        sphereTex.bind(0);
        program_envmap.setUniform("envmap", 0);
        program_envmap.setUniformMatrixPtr<4>("MP", &MP[0][0]);

        mogl::setCullFace(GL_FRONT);
        mesh.draw(program_envmap, Mesh::Vertex | Mesh::UV);
        mogl::setCullFace(GL_BACK);

        mogl::enable(GL_DEPTH_TEST);

        program_render.use();
        vao.bind();

        ubo_transform.bindBufferBase(0);
        struct transforms_t
        {
            glm::mat4 mat_proj;
            glm::mat4 mat_view;
            glm::mat4 mat_model[sphereCount];
        } * transforms = static_cast<transforms_t*>(ubo_transform.mapRange(0, sizeof(transforms_t), static_cast<GLbitfield>(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)));
        transforms->mat_proj = glm::perspective(70.0f, (float)ctx.getWindowSize().x / (float)ctx.getWindowSize().y, 1.0f, 1000.0f);
        transforms->mat_view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -20.0f));
        for (int i = 0; i < sphereCount; i++)
        {
            float fi = 3.141592f * (float)i / 16.0f;
            // float r = cosf(fi * 0.25f) * 0.4f + 1.0f;
            float r = (i & 2) ? 0.6f : 1.5f;
            transforms->mat_model[i] = glm::translate(glm::mat4(1.0f), glm::vec3(cosf(t + fi) * 5.0f * r, sinf(t + fi * 4.0f) * 4.0f, sinf(t + fi) * 5.0f * r));
        }
        ubo_transform.unmap();

        ubo_material.bindBufferBase(1);

        sphereTex.bind(0);

        glUniform1f(uniforms.scene.bloom_thresh_min, bloom_thresh_min);
        glUniform1f(uniforms.scene.bloom_thresh_max, bloom_thresh_max);
        program_render.setUniform("envmap", 0);

        mesh.draw(program_render, Mesh::Vertex | Mesh::Normal | Mesh::UV, sphereCount);

        mogl::disable(GL_DEPTH_TEST);

        program_filter.use();

        vao.bind();

        filter_fbo[0].bind(GL_FRAMEBUFFER);

        tex_brightpass.bind(0);
        mogl::setViewport(0, 0, ctx.getWindowSize().y, ctx.getWindowSize().x);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        filter_fbo[1].bind(GL_FRAMEBUFFER);

        tex_filter[0].bind(0);
        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        program_resolve.use();

        glUniform1f(uniforms.resolve.exposure, exposure);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        tex_scene.bind(0);
        tex_filter[1].bind(1);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // 1. Show a simple window
        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
        {
            static float f;
            ImGui::Text("Hello, world!");
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float*)&clear_color);
            if (ImGui::Button("Test Window")) show_test_window ^= 1;
            if (ImGui::Button("Another Window")) show_another_window ^= 1;
        }

        // 2. Show another simple window, this time using an explicit Begin/End pair
        if (show_another_window)
        {
            ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello");
            ImGui::End();
        }

        // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
        if (show_test_window)
        {
            ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
            ImGui::ShowTestWindow(&show_test_window);
        }

        // Rendering
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui::Render();
        ctx.swapBuffers();

    } while(ctx.isOpen());
}

int main(int /*ac*/, char** /*av*/)
{
    auto debugCallback = [] (GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *message, const void *) { std::cout << "GL: " << message << std::endl; };
    try {
        GLContext   ctx;
        SixAxis     controller("/dev/input/js0");

        ctx.create(1600, 900, 4, 5, false, true);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, nullptr);
        GLuint unusedIds = 0;
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &unusedIds, GL_TRUE);
//         hdr_test(ctx, controller);
        bloom_test(ctx, controller);
        controller.destroy();
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
