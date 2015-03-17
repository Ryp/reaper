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

#include <deps/object.h> // FIXME remove

void    hdr_test(GLContext& ctx, SixAxis& controller)
{
    ResourceManager     resourceMgr("rc");
    float               frameTime;
    Camera              cam(glm::vec3(-5.0, 3.0, 0.0));
    mogl::VertexArray   vao;
    Model*              quad = resourceMgr.getModel("model/quad.obj");
    Mesh                mesh(resourceMgr.getModel("model/sphere.obj"));
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

    // Frame buffer for the shadow map
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

    // Frame buffer for the normals / depth
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

    vao.bind();

    mesh.init();

    quadBuffer.bind();
    quadBuffer.setData(quad->getVertexBufferSize(), quad->getVertexBuffer(), GL_STATIC_DRAW);

    modelTexture.bind(0);
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

void    bloom_test(GLContext& ctx, SixAxis& /*controller*/)
{
    mogl::FrameBuffer   render_fbo;
    mogl::FrameBuffer   filter_fbo[2];
    mogl::Texture       tex_scene(GL_TEXTURE_2D);
    mogl::Texture       tex_brightpass(GL_TEXTURE_2D);
    mogl::Texture       tex_depth(GL_TEXTURE_2D);
    mogl::Texture       tex_filter[2] = {GL_TEXTURE_2D, GL_TEXTURE_2D};
    mogl::Texture       tex_lut(GL_TEXTURE_1D);
    mogl::ShaderProgram program_render;
    mogl::ShaderProgram program_filter;
    mogl::ShaderProgram program_resolve;
    mogl::VertexArray   vao;
    mogl::Buffer        ubo_transform(GL_UNIFORM_BUFFER);
    mogl::Buffer        ubo_material(GL_UNIFORM_BUFFER);
    float       exposure(1.0f);
    bool        paused(false);
    float       bloom_factor(1.0f);
    bool        show_bloom(true);
    bool        show_scene(true);
    bool        show_prefilter(false);
    float       bloom_thresh_min(0.8f);
    float       bloom_thresh_max(1.2f);
    object      object;

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
            int bloom_factor;
            int scene_factor;
        } resolve;
    } uniforms;

    enum
    {
        MAX_SCENE_WIDTH     = 2048,
        MAX_SCENE_HEIGHT    = 2048,
        SPHERE_COUNT        = 32,
    };

    static const GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    static const GLfloat exposureLUT[20]   = { 11.0f, 6.0f, 3.2f, 2.8f, 2.2f, 1.90f, 1.80f, 1.80f, 1.70f, 1.70f,  1.60f, 1.60f, 1.50f, 1.50f, 1.40f, 1.40f, 1.30f, 1.20f, 1.10f, 1.00f };

    vao.bind();

    ShaderHelper::loadSimpleShader(program_render, "rc/shader/hdrbloom/hdrbloom-scene.vs.glsl", "rc/shader/hdrbloom/hdrbloom-scene.fs.glsl");
    uniforms.scene.bloom_thresh_min = program_render.getUniformLocation("bloom_thresh_min");
    uniforms.scene.bloom_thresh_max = program_render.getUniformLocation("bloom_thresh_max");
    ShaderHelper::loadSimpleShader(program_filter, "rc/shader/hdrbloom/hdrbloom-filter.vs.glsl", "rc/shader/hdrbloom/hdrbloom-filter.fs.glsl");
    ShaderHelper::loadSimpleShader(program_resolve, "rc/shader/hdrbloom/hdrbloom-resolve.vs.glsl", "rc/shader/hdrbloom/hdrbloom-resolve.fs.glsl");
    uniforms.resolve.exposure = program_resolve.getUniformLocation("exposure");
    uniforms.resolve.bloom_factor = program_resolve.getUniformLocation("bloom_factor");
    uniforms.resolve.scene_factor = program_resolve.getUniformLocation("scene_factor");


    tex_scene.setStorage2D(1, GL_RGBA16F, MAX_SCENE_WIDTH, MAX_SCENE_HEIGHT);
    tex_brightpass.setStorage2D(1, GL_RGBA16F, MAX_SCENE_WIDTH, MAX_SCENE_HEIGHT);
    tex_depth.setStorage2D(1, GL_DEPTH_COMPONENT32F, MAX_SCENE_WIDTH, MAX_SCENE_HEIGHT);
    render_fbo.setTexture(GL_COLOR_ATTACHMENT0, tex_scene, 0);
    render_fbo.setTexture(GL_COLOR_ATTACHMENT1, tex_brightpass, 0);
    render_fbo.setTexture(GL_DEPTH_ATTACHMENT, tex_depth, 0);
    render_fbo.setDrawBuffers(2, buffers);

    for (int i = 0; i < 2; ++i)
    {
        tex_filter[i].setStorage2D(1, GL_RGBA16F, i ? MAX_SCENE_WIDTH : MAX_SCENE_HEIGHT, i ? MAX_SCENE_HEIGHT : MAX_SCENE_WIDTH);
        filter_fbo[i].setTexture(GL_COLOR_ATTACHMENT0, tex_filter[i], 0);
        filter_fbo[i].setDrawBuffers(1, buffers);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    tex_lut.setStorage1D(1, GL_R32F, 20);
    tex_lut.setSubImage1D(0, 0, 20, GL_RED, GL_FLOAT, exposureLUT);
    tex_lut.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex_lut.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex_lut.set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

    object.load("rc/model/sphere.sbm");

    ubo_transform.bind();
    glBufferData(GL_UNIFORM_BUFFER, (2 + SPHERE_COUNT) * sizeof(glm::mat4), NULL, GL_DYNAMIC_DRAW);

    struct material
    {
        glm::vec3     diffuse_color;
        unsigned int    : 32;           // pad
        glm::vec3     specular_color;
        float           specular_power;
        glm::vec3     ambient_color;
        unsigned int    : 32;           // pad
    };

    ubo_material.bind();
    glBufferData(GL_UNIFORM_BUFFER, SPHERE_COUNT * sizeof(material), NULL, GL_STATIC_DRAW);

    material * m = (material *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, SPHERE_COUNT * sizeof(material), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    float ambient = 0.002f;
    for (int i = 0; i < SPHERE_COUNT; ++i)
    {
        float fi = 3.14159267f * (float)i / 8.0f;
        m[i].diffuse_color  = glm::vec3(sinf(fi) * 0.5f + 0.5f, sinf(fi + 1.345f) * 0.5f + 0.5f, sinf(fi + 2.567f) * 0.5f + 0.5f);
        m[i].specular_color = glm::vec3(2.8f, 2.8f, 2.9f);
        m[i].specular_power = 30.0f;
        m[i].ambient_color  = glm::vec3(ambient * 0.025f);
        ambient *= 1.5f;
    }
    glUnmapBuffer(GL_UNIFORM_BUFFER);
    do
    {
        float currentTime = ctx.getTime();
        static const GLfloat black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        static const GLfloat one = 1.0f;
        int i;
        static double last_time = 0.0;
        static double total_time = 0.0;

        if (!paused)
            total_time += (currentTime - last_time);
        last_time = currentTime;
        float t = (float)total_time;

        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);

        render_fbo.bind(GL_FRAMEBUFFER);
        render_fbo.clear(GL_COLOR, 0, black);
        render_fbo.clear(GL_COLOR, 1, black);
        render_fbo.clear(GL_DEPTH, 0, &one);

        mogl::enable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        program_render.use();

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_transform.getHandle());
        struct transforms_t
        {
            glm::mat4 mat_proj;
            glm::mat4 mat_view;
            glm::mat4 mat_model[SPHERE_COUNT];
        } * transforms = (transforms_t *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(transforms_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        transforms->mat_proj = glm::perspective(70.0f, (float)ctx.getWindowSize().x / (float)ctx.getWindowSize().y, 1.0f, 1000.0f);
        transforms->mat_view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -20.0f));
        for (i = 0; i < SPHERE_COUNT; i++)
        {
            float fi = 3.141592f * (float)i / 16.0f;
            // float r = cosf(fi * 0.25f) * 0.4f + 1.0f;
            float r = (i & 2) ? 0.6f : 1.5f;
            transforms->mat_model[i] = glm::translate(glm::mat4(1.0f), glm::vec3(cosf(t + fi) * 5.0f * r, sinf(t + fi * 4.0f) * 4.0f, sinf(t + fi) * 5.0f * r));
        }
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_material.getHandle());

        glUniform1f(uniforms.scene.bloom_thresh_min, bloom_thresh_min);
        glUniform1f(uniforms.scene.bloom_thresh_max, bloom_thresh_max);

        object.render(SPHERE_COUNT);

        mogl::disable(GL_DEPTH_TEST);

        program_filter.use();

        vao.bind();

        filter_fbo[0].bind(GL_FRAMEBUFFER);

        tex_brightpass.bind(0);
        glViewport(0, 0, ctx.getWindowSize().y, ctx.getWindowSize().x);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        filter_fbo[1].bind(GL_FRAMEBUFFER);

        tex_filter[0].bind(0);
        glViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        program_resolve.use();

        glUniform1f(uniforms.resolve.exposure, exposure);
        if (show_prefilter)
        {
            glUniform1f(uniforms.resolve.bloom_factor, 0.0f);
            glUniform1f(uniforms.resolve.scene_factor, 1.0f);
        }
        else
        {
            glUniform1f(uniforms.resolve.bloom_factor, show_bloom ? bloom_factor : 0.0f);
            glUniform1f(uniforms.resolve.scene_factor, show_scene ? 1.0f : 0.0f);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        tex_filter[1].bind(1);
        if (show_prefilter)
            tex_brightpass.bind(0);
        else
            tex_scene.bind(0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
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
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
