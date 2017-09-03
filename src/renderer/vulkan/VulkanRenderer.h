////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "mesh/MeshCache.h"

#include "renderer/Renderer.h"
#include "renderer/texture/TextureCache.h"

#include "api/Vulkan.h"
#include "core/DynamicLibrary.h"

#include "SwapchainRendererBase.h"

struct VSUBO;

class OldRenderer : public SwapchainRendererBase
{
    using parent_type = SwapchainRendererBase;

public:
    OldRenderer();
    ~OldRenderer() = default;

    virtual void startup(IWindow* window) override final;
    virtual void shutdown() override final;
    virtual void render() override final;

private:
    void createCommandBuffers();
    void createDescriptorPool();
    void createPipeline();
    void createMeshBuffers();
    void createTextures();
    void createDescriptorSet();

private:
    void updateUniforms();

private:
    VkPipeline       _pipeline;
    VkPipelineLayout _pipelineLayout;

private:
    VkDeviceMemory _deviceMemory;

    VkDeviceMemory _texMemory;
    VkImage        _texImage;
    VkImageView    _texImageView;
    VkSampler      _texSampler;

    VkBuffer _vertexBuffer;
    VkBuffer _indexBuffer;

private:
    VkDescriptorPool      _descriptorPool;
    VkDescriptorSet       _descriptorSet;
    VkDescriptorSetLayout _descriptorSetLayout;

private:
    struct
    {
        VkBuffer               buffer;
        VkDeviceMemory         memory;
        VkDescriptorBufferInfo descriptor;
    } _uniformData;

private:
    VSUBO*       _uniforms;
    MeshCache    _meshCache;
    TextureCache _texCache;
};

void test_vulkan_renderer(ReaperRoot& root, VulkanBackend& backend);
