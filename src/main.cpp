////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015 Thibault Schueller
///
/// @file main.cpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
////////////////////////////////////////////////////////////////////////////////

#include "pipeline/glcontext.hpp"

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

#define GLM_FORCE_RADIANS // NOTE remove this when switching to glm 0.9.6 or above
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
#include "filesystem/path.hpp"

void    hdr_test(GLContext& ctx, SixAxis& controller, const std::string& rcPath)
{
    ResourceManager     resourceMgr(rcPath);
    float               frameTime;
    Camera              cam(glm::vec3(-5.0, 3.0, 0.0));
    mogl::VAO           vao;
    Model*              quad = resourceMgr.getModel("model/quad.obj");
    Mesh                mesh(resourceMgr.getModel("model/sibenik_bare.obj"));
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

    ShaderHelper::loadSimpleShader(depthpassshader, rcPath + "shader/depth_pass.vert", rcPath + "shader/depth_pass.frag");
    ShaderHelper::loadSimpleShader(normdepthpassshader, rcPath + "shader/depth_pass_normalized.vert", rcPath + "shader/depth_pass_normalized.frag");
    ShaderHelper::loadSimpleShader(shader, rcPath + "shader/shadow_ssao.vert", rcPath + "shader/shadow_ssao.frag");
    ShaderHelper::loadSimpleShader(postshader, rcPath + "shader/post/passthrough.vert", rcPath + "shader/post/tonemapping.frag");

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
    Assert(postFrameBuffer.isComplete(GL_FRAMEBUFFER));

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
    Assert(shadowDepthFrameBuffer.isComplete(GL_FRAMEBUFFER));

    // Frame buffer for the normals / depth
    screenNormalTexture.setStorage2D(1, GL_RGB8, ctx.getWindowSize().x, ctx.getWindowSize().y);
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
    screenDepthTexture.bind(1);
    screenDepthFrameBuffer.setTexture(GL_COLOR_ATTACHMENT0, screenNormalTexture);
    screenDepthFrameBuffer.setTexture(GL_DEPTH_ATTACHMENT, screenDepthTexture);
    screenDepthFrameBuffer.setDrawBuffer(GL_COLOR_ATTACHMENT0);
    Assert(screenDepthFrameBuffer.isComplete(GL_FRAMEBUFFER));

    vao.bind();

    mesh.init();

    quadBuffer.bind();
    quadBuffer.setData(quad->getVertexBufferSize(), quad->getVertexBuffer(), GL_STATIC_DRAW);

    ImageLoader::loadDDS(rcPath + "texture/default.dds", modelTexture);

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

        mogl::enable(GL_CULL_FACE);
        Assert(mogl::isEnabled(GL_CULL_FACE));

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

void    bloom_test(GLContext& ctx, SixAxis& controller, const std::string& rcPath)
{
    mogl::FrameBuffer   render_fbo;
    mogl::FrameBuffer   blur1_render_fbo;
    mogl::FrameBuffer   blur2_render_fbo;
    mogl::FrameBuffer   filter_fbo[2];
    mogl::FrameBuffer   post_fbo[2];
    mogl::Texture       tex_scene(GL_TEXTURE_2D);
    mogl::Texture       tex_back_scene(GL_TEXTURE_2D);
    mogl::Texture       tex_brightpass(GL_TEXTURE_2D);
    mogl::Texture       tex_depth(GL_TEXTURE_2D);
    mogl::Texture       tex_lut(GL_TEXTURE_1D);
    mogl::Texture       tex_filter[2] = {GL_TEXTURE_2D, GL_TEXTURE_2D};
    mogl::Texture       sphereTex(GL_TEXTURE_2D);
    mogl::Texture       bumpMap(GL_TEXTURE_2D);
    mogl::Texture       envMapHDR(GL_TEXTURE_2D);
    mogl::Texture       tex_post[2] = {GL_TEXTURE_2D, GL_TEXTURE_2D};
    mogl::Texture       tex_3d_lut(GL_TEXTURE_3D);
    mogl::Texture       tex_2d_lut(GL_TEXTURE_2D);
    mogl::RenderBuffer  depth_post;
    mogl::ShaderProgram program_envmap;
    mogl::ShaderProgram program_render;
    mogl::ShaderProgram program_filter;
    mogl::ShaderProgram program_resolve;
    mogl::ShaderProgram program_blur;
    mogl::ShaderProgram program_post;
    mogl::VAO           vao;
    mogl::UniformBuffer ubo_transform;
    mogl::UniformBuffer ubo_material;
    mogl::Query         timeQuery(GL_TIME_ELAPSED);
    Camera              camera;
    ResourceManager     resourceMgr(rcPath);
    Mesh                mesh(resourceMgr.getModel("model/sphere.obj"));
    Mesh                sphere(resourceMgr.getModel("model/sphere.obj"));
    float               exposure(1.0f);
    float               fx_force(1.0f);
    bool                bloom(false);
    bool                post_process(true);
    bool                blurImage(false);
    float               bloom_thresh_min(0.0f);
    float               bloom_thresh_max(1.5f);
    const int           meshN = 7;
    const int           meshTotal = meshN * meshN;
    float               fovAngle = 55.0f;
    glm::vec3           lightPos(0.0f, 5.5f, 0.0f);
    float               lightIntensity(200.0f);

    Assert(ctx.isExtensionSupported("GL_EXT_texture_filter_anisotropic"));

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
        glm::vec3       albedo;
        float           fresnel;
        float           roughness;
        unsigned int    : 32;
        unsigned int    : 32;
        unsigned int    : 32;
    };

    GLfloat maxAnisotropy = mogl::get<GLfloat>(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);

    std::cout << "MaxAnisotropy=" << maxAnisotropy << std::endl;

    ImageLoader::loadDDS3D(rcPath + "texture/lut/color_grading_contrast.dds", tex_3d_lut);
    ImageLoader::loadDDS(rcPath + "texture/lut/color_grading_contrast.dds", tex_2d_lut);
    ImageLoader::loadDDS(rcPath + "texture/metal_normal.dds", bumpMap);
    ImageLoader::loadDDS(rcPath + "texture/envmap/lobby_mip.dds", sphereTex);
    ImageLoader::loadEXR(rcPath + "texture/envmap/StageEnvLatLong.exr", envMapHDR);

    tex_3d_lut.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex_3d_lut.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex_3d_lut.set(GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    tex_3d_lut.set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    tex_3d_lut.set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    tex_2d_lut.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex_2d_lut.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex_2d_lut.set(GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    tex_2d_lut.set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    sphereTex.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    sphereTex.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    sphereTex.set(GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
    bumpMap.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Linear filtering on a normal map ~
    bumpMap.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    bumpMap.set(GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);

    static const GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    static const GLfloat exposureLUT[20]   = { 11.0f, 6.0f, 3.2f, 2.8f, 2.2f, 1.90f, 1.80f, 1.80f, 1.70f, 1.70f,  1.60f, 1.60f, 1.50f, 1.50f, 1.40f, 1.40f, 1.30f, 1.20f, 1.10f, 1.00f };

    vao.bind();

    ShaderHelper::loadSimpleShader(program_envmap, rcPath + "shader/envmap.vert", rcPath + "shader/envmap.frag");
    ShaderHelper::loadSimpleShader(program_render, rcPath + "shader/hdrbloom/hdrbloom-scene.vs.glsl", rcPath + "shader/hdrbloom/hdrbloom-scene.fs.glsl");
    uniforms.scene.bloom_thresh_min = program_render.getUniformLocation("bloom_thresh_min");
    uniforms.scene.bloom_thresh_max = program_render.getUniformLocation("bloom_thresh_max");
    ShaderHelper::loadSimpleShader(program_filter, rcPath + "shader/hdrbloom/hdrbloom-filter.vs.glsl", rcPath + "shader/hdrbloom/hdrbloom-filter.fs.glsl");
    ShaderHelper::loadSimpleShader(program_resolve, rcPath + "shader/hdrbloom/hdrbloom-resolve.vs.glsl", rcPath + "shader/hdrbloom/hdrbloom-resolve.fs.glsl");
    uniforms.resolve.exposure = program_resolve.getUniformLocation("exposure");

    ShaderHelper::loadSimpleShader(program_blur, rcPath + "shader/gaussian_blur.vert", rcPath + "shader/gaussian_blur.frag");
    //     ShaderHelper::loadSimpleShader(program_post, rcPath + "shader/post/passthrough2.vert", rcPath + "shader/post/chromatic_aberrations.frag");
    //     ShaderHelper::loadSimpleShader(program_post, rcPath + "shader/post/passthrough2.vert", rcPath + "shader/post/color_grading.frag");
    ShaderHelper::loadSimpleShader(program_post, rcPath + "shader/post/passthrough2.vert", rcPath + "shader/post/color_grading2d.frag");

    depth_post.setStorage(GL_DEPTH_COMPONENT, ctx.getWindowSize().x, ctx.getWindowSize().y);
    for (int i = 0; i < 2; ++i)
    {
        tex_post[i].setStorage2D(1, GL_RGB8, ctx.getWindowSize().x, ctx.getWindowSize().y);
        tex_post[i].set(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        tex_post[i].set(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        tex_post[i].set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        tex_post[i].set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        tex_post[i].bind(0);
        post_fbo[i].setTexture(GL_COLOR_ATTACHMENT0, tex_post[i]);
        post_fbo[i].setRenderBuffer(GL_DEPTH_ATTACHMENT, depth_post);
        post_fbo[i].setDrawBuffer(GL_COLOR_ATTACHMENT0);
        Assert(post_fbo[i].isComplete(GL_FRAMEBUFFER));
    }

    tex_scene.setStorage2D(1, GL_RGBA16F, ctx.getWindowSize().x, ctx.getWindowSize().y);
    tex_scene.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR); // For blur pass
    tex_scene.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR); // For blur pass
    tex_scene.set(GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_EDGE); // For blur pass
    tex_scene.set(GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_TO_EDGE); // For blur pass

    tex_brightpass.setStorage2D(11, GL_RGBA16F, ctx.getWindowSize().x, ctx.getWindowSize().y);
    tex_brightpass.generateMipmap();
    tex_depth.setStorage2D(1, GL_DEPTH_COMPONENT32F, ctx.getWindowSize().x, ctx.getWindowSize().y);
    render_fbo.setTexture(GL_COLOR_ATTACHMENT0, tex_scene, 0);
    render_fbo.setTexture(GL_COLOR_ATTACHMENT1, tex_brightpass, 0);
    render_fbo.setTexture(GL_DEPTH_ATTACHMENT, tex_depth, 0);
    render_fbo.setDrawBuffers(2, buffers);
    Assert(render_fbo.isComplete(GL_FRAMEBUFFER));

    tex_back_scene.setStorage2D(1, GL_RGBA16F, ctx.getWindowSize().x, ctx.getWindowSize().y);
    tex_back_scene.set(GL_TEXTURE_MIN_FILTER, GL_LINEAR); // For blur pass
    tex_back_scene.set(GL_TEXTURE_MAG_FILTER, GL_LINEAR); // For blur pass
    tex_back_scene.set(GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_EDGE); // For blur pass
    tex_back_scene.set(GL_TEXTURE_WRAP_T, GL_MIRROR_CLAMP_TO_EDGE); // For blur pass

    blur1_render_fbo.setTexture(GL_COLOR_ATTACHMENT0, tex_back_scene, 0);
    blur2_render_fbo.setTexture(GL_COLOR_ATTACHMENT0, tex_scene, 0);
    Assert(blur1_render_fbo.isComplete(GL_FRAMEBUFFER));
    Assert(blur2_render_fbo.isComplete(GL_FRAMEBUFFER));

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
    sphere.init();

    ubo_transform.setData((2 + meshTotal) * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
    ubo_material.setData(meshTotal * sizeof(material), nullptr, GL_STATIC_DRAW);
    material* m = static_cast<material*>(ubo_material.mapRange(0, meshTotal * sizeof(material), static_cast<GLbitfield>(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)));
    for (int i = 0; i < meshN; ++i)
    {
        for (int j = 0; j < meshN; ++j)
        {
            int index = i * meshN + j;
            m[index].fresnel        = static_cast<float>(j + 1) / static_cast<float>(meshN);
            m[index].roughness      = static_cast<float>(i + 1) / static_cast<float>(meshN);
            m[index].albedo         = glm::vec3(1.0f - m[index].roughness, 1.0f - m[index].fresnel, m[index].roughness);
        }
    }
    ubo_material.unmap();

    camera.setPosition(glm::vec3(-8.0f, 6.0f, -8.0f));
    camera.setRotation(glm::quarter_pi<float>(), -glm::quarter_pi<float>() * 0.95f);
    camera.update(Camera::Spherical);

    mogl::setCullFace(GL_BACK);
    glDepthFunc(GL_LESS);
    do
    {
        static const GLfloat black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        static const GLfloat one = 1.0f;

        mogl::enable(GL_CULL_FACE);
        Assert(mogl::isEnabled(GL_CULL_FACE));
        timeQuery.begin();
        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplGlfwGL3_NewFrame();

        controller.update();
        if (controller.isHeld(SixAxis::Buttons::Start))
            break;
        float speedup = 1.0 + (controller.getAxis(SixAxis::Axes::L2Pressure) * 0.5 + 0.5) * 10.0f;
        camera.move(controller.getAxis(SixAxis::LeftAnalogY) * speedup / -10.0f, controller.getAxis(SixAxis::LeftAnalogX) * speedup / -10.0f, 0.0f);
        camera.rotate(controller.getAxis(SixAxis::RightAnalogX) / 10.0f, controller.getAxis(SixAxis::RightAnalogY) / -10.0f);
        if (controller.isPressed(SixAxis::Buttons::Select))
            camera.reset();
        camera.update(Camera::Spherical);

        mogl::setViewport(0, 0, ctx.getWindowSize().x, ctx.getWindowSize().y);
        render_fbo.bind(GL_FRAMEBUFFER);
        render_fbo.clear(GL_COLOR, 0, black);
        render_fbo.clear(GL_COLOR, 1, black);
        render_fbo.clear(GL_DEPTH, 0, &one);

        glm::mat4   Projection = glm::perspective(glm::radians(fovAngle), (float)ctx.getWindowSize().x / (float)ctx.getWindowSize().y, 0.1f, 100.0f);
        glm::mat4   Model = glm::scale(glm::mat4(1.0f), glm::vec3(50.0f, 50.0f, 50.0f));
//         glm::mat4   View = glm::lookAt(glm::vec3(-8.0f, 6.0f, -8.0f), glm::vec3(0.0f, -4.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4   View = camera.getViewMatrix();
        glm::mat4   MVP = Projection * View * Model;

        mogl::disable(GL_DEPTH_TEST);
        program_envmap.use();
        vao.bind();

        envMapHDR.bind(0);
        program_envmap.setUniform("envmap", 0);
        program_envmap.setUniformMatrixPtr<4>("MVP", &MVP[0][0]);
        mogl::setCullFace(GL_FRONT);
        sphere.draw(program_envmap, Mesh::Vertex | Mesh::Normal);
        mogl::setCullFace(GL_BACK);

        mogl::enable(GL_DEPTH_TEST);

        program_render.use();
        vao.bind();

        ubo_transform.bindBufferBase(0);
        struct transforms_t
        {
            glm::mat4 mat_proj;
            glm::mat4 mat_view;
            glm::mat4 mat_model[meshTotal];
        } * transforms = static_cast<transforms_t*>(ubo_transform.mapRange(0, sizeof(transforms_t), static_cast<GLbitfield>(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)));
        transforms->mat_proj = Projection;
        transforms->mat_view = View;
        for (int i = 0; i < meshN; ++i)
        {
            for (int j = 0; j < meshN; ++j)
            {
                int index = i * meshN + j;
                transforms->mat_model[index] = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3((j - meshN / 2) * 2.0f, 0.0f, (i - meshN / 2) * 2.0f)), static_cast<float>(M_PI) * -0.25f, glm::vec3(0.0f, 1.0f, 0.0f));
            }
        }
        ubo_transform.unmap();

        ubo_material.bindBufferBase(1);

        envMapHDR.bind(0);
        bumpMap.bind(1);
        glUniform1f(uniforms.scene.bloom_thresh_min, bloom_thresh_min);
        glUniform1f(uniforms.scene.bloom_thresh_max, bloom_thresh_max);
//         program_render.setUniform("envmap", 0);
        program_render.setUniform("normal_tex", 1);
        program_render.setUniformPtr<3>("light_pos", &lightPos[0]);
        program_render.setUniform("light_power", lightIntensity);

        mesh.draw(program_render, Mesh::Vertex | Mesh::Normal | Mesh::UV | Mesh::Tangent, meshTotal);

        if (!bloom)
            render_fbo.clear(GL_COLOR, 1, black);

        mogl::disable(GL_DEPTH_TEST);

        if (blurImage)
        {
            blur1_render_fbo.bind(GL_FRAMEBUFFER);
            blur1_render_fbo.clear(GL_COLOR, 0, black);
            tex_scene.bind(0);
            program_blur.use();
            program_blur.setUniform<GLfloat>("imageSize", ctx.getWindowSize().x, ctx.getWindowSize().y);

            program_blur.setUniformSubroutine(GL_FRAGMENT_SHADER, "blurSelector", "blurV");
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            blur2_render_fbo.bind(GL_FRAMEBUFFER);
            blur2_render_fbo.clear(GL_COLOR, 0, black);

            program_blur.setUniformSubroutine(GL_FRAGMENT_SHADER, "blurSelector", "blurH");
            tex_back_scene.bind(0);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

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

        {
            if (post_process)
            {
                post_fbo[0].bind(GL_FRAMEBUFFER);
                post_fbo[0].clear(GL_COLOR, 0, black);
                post_fbo[0].clear(GL_DEPTH, 0, &one);
            }
            else
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

            program_resolve.use();

            glUniform1f(uniforms.resolve.exposure, exposure);

            tex_scene.bind(0);
            tex_filter[1].bind(1);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        if (post_process)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            program_post.use();
            program_post.setUniform<GLfloat>("imageSize", ctx.getWindowSize().x, ctx.getWindowSize().y);
//             program_post.setUniform("amount", glm::exp(fx_force) - 1.0f);
            program_post.setUniform("lutSize", 16.0f);

            tex_post[0].bind(0);
            tex_2d_lut.bind(1);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        timeQuery.end();

        ImGui::Begin("ReaperGL");
        ImGui::Text("frameTime=%.4f ms", static_cast<double>(timeQuery.get<GLuint>(GL_QUERY_RESULT)) * 0.000001);
//         ImGui::PlotHistogram("frameTime", );
        if (ImGui::CollapsingHeader("Camera"))
        {
            ImGui::SliderFloat("fovAngle", &fovAngle, 5.0f, 170.0f);
        }
        if (ImGui::CollapsingHeader("PostFX"))
        {
            ImGui::Checkbox("Bloom", &bloom);
            ImGui::SliderFloat("bloom_thresh_min", &bloom_thresh_min, 0.0f, 5.0f);
            ImGui::SliderFloat("bloom_thresh_max", &bloom_thresh_max, 0.0f, 5.0f);
            ImGui::SliderFloat("exposure", &exposure, 0.0f, 10.0f);
            ImGui::Checkbox("Blur", &blurImage);
            ImGui::Checkbox("Post_FX", &post_process);
            ImGui::SliderFloat("FX force", &fx_force, 0.0f, 2.0f);
        }
        if (ImGui::CollapsingHeader("Light"))
        {
            ImGui::SliderFloat3("light", &lightPos[0], -20.0f, 20.0f);
            ImGui::SliderFloat("intensity", &lightIntensity, 0.0f, 1000.0f);
        }
        ImGui::End();

        // Rendering
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui::Render();
        ctx.swapBuffers();

    } while(ctx.isOpen());
}

int main(int /*ac*/, char** av)
{
    auto debugCallback = [] (GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *message, const void *) {
        std::cout << "glCallback: " << message << std::endl;
//         Assert(false, std::string("glCallback: ") + message);
    };
    try {
        GLContext   ctx;
        SixAxis     controller("/dev/input/js0");
        ctx.create(1600, 900, 4, 5, false, true);

        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

        GLuint usedIds = 131185;
        glDebugMessageCallback(debugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 1, &usedIds, GL_FALSE);

//         bloom_test(ctx, controller, reaper::getExecutablePath(av[0]) + "../rc/");
        hdr_test(ctx, controller, reaper::getExecutablePath(av[0]) + "../rc/");
        controller.destroy();
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
