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
#include "api/Vulkan.h"
#include "core/DynamicLibrary.h"

#include "SwapchainRendererBase.h"

struct VSUBO;

class VulkanRenderer : public SwapchainRendererBase
{
    using parent_type = SwapchainRendererBase;
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() = default;

    virtual void startup(Window* window) override final;
    virtual void shutdown() override final;
    virtual void render() override final;

private:
    void createCommandBuffers();
    void createDescriptorPool();
    void createPipeline();
    void createMeshBuffers();
    void createDescriptorSet();

private:
    void updateUniforms();

private:
    VkPipeline          _pipeline;
    VkPipelineLayout    _pipelineLayout;

private:
    VkDeviceMemory  _deviceMemory;
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
    VSUBO*      _uniforms;
    MeshCache   _cache;
};

#endif // REAPER_VULKANRENDERER_INCLUDED
