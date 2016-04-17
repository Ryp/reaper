////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_VULKANRENDERER_INCLUDED
#define REAPER_VULKANRENDERER_INCLUDED

#include <vector>

#include "renderer/Renderer.h"
#include "api/Vulkan.h"
#include "core/DynamicLibrary.h"

class VulkanRenderer : public AbstractRenderer
{
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() = default;

public:
    void startup(Window* window) override;
    void shutdown() override;
    void render() override;

private:
    void createSemaphores();
    void createSwapChain();
    void createCommandBuffers();
    void recordCommandBuffers();

private:
    LibHandle           _vulkanLib;

private:
    VkInstance          _instance;
    VkPhysicalDevice    _physicalDevice;
    VkSemaphore         _imageAvailableSemaphore;
    VkSemaphore         _renderingFinishedSemaphore;
    VkSurfaceKHR        _presentationSurface;
    VkQueue             _presentationQueue;
    VkSwapchainKHR      _swapChain;
    VkDevice            _device;

private:
    VkCommandPool                   _presentCmdPool;
    std::vector<VkCommandBuffer>    _presentCmdBuffers;
    uint32_t                        _presentQueueIndex;
};

#endif // REAPER_VULKANRENDERER_INCLUDED
