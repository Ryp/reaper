////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_VULKANRENDERER_INCLUDED
#define REAPER_VULKANRENDERER_INCLUDED

#include <vector>

#include "renderer/Renderer.h"
#include "renderer/mesh/MeshCache.h"
#include "renderer/texture/TextureCache.h"

#include "api/Vulkan.h"
#include "core/DynamicLibrary.h"

#include "SwapchainRendererBase.h"

struct VSUBO;

class VulkanRenderer : public SwapchainRendererBase
{
    using parent_type = SwapchainRendererBase;
public:
    VulkanRenderer();
    ~VulkanRenderer() = default;

    virtual void startup(Window* window) override final;
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
    VkPipeline          _pipeline;
    VkPipelineLayout    _pipelineLayout;

private:
    VkDeviceMemory  _deviceMemory;

    VkDeviceMemory  _texMemory;
    VkImage         _texImage;
    VkImageView     _texImageView;
    VkSampler       _texSampler;

    VkBuffer        _vertexBuffer;
    VkBuffer        _indexBuffer;

private:
    VkDescriptorPool        _descriptorPool;
    VkDescriptorSet         _descriptorSet;
    VkDescriptorSetLayout   _descriptorSetLayout;

private:
    struct {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDescriptorBufferInfo descriptor;
    }  _uniformData;

private:
    VSUBO*          _uniforms;
    MeshCache       _meshCache;
    TextureCache    _texCache;
};

#endif // REAPER_VULKANRENDERER_INCLUDED
