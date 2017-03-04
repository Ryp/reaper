////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_SWAPCHAINRENDERERBASE_INCLUDED
#define REAPER_SWAPCHAINRENDERERBASE_INCLUDED

#include <vector>

#include "core/DynamicLibrary.h"
#include "api/Vulkan.h"
#include "renderer/Renderer.h"

class SwapchainRendererBase : public AbstractRenderer
{
public:
    SwapchainRendererBase() = default;
    ~SwapchainRendererBase() = default;

    virtual void startup(Window* window) override;
    virtual void shutdown() override;

private:
    void createSemaphores();
    void createSwapChain();
    void createRenderPass();
    void createFramebuffers();
    void createCommandBufferPool();

private:
    LibHandle   _vulkanLib;

private:
    VkDebugReportCallbackEXT _debugCallback;

protected:
    VkPhysicalDevice    _physicalDevice;
    VkSurfaceKHR        _presentationSurface;
    VkQueue             _presentationQueue;
    VkSwapchainKHR      _swapChain;
    u32                 _swapChainImageCount;
    VkFormat            _swapChainFormat;
    std::vector<VkImage>        _swapChainImages;
    std::vector<VkImageView>    _swapChainImageViews;
    std::vector<VkFramebuffer>  _framebuffers;
    VkSemaphore                 _imageAvailableSemaphore;
    VkSemaphore                 _renderingFinishedSemaphore;
    VkRenderPass                _mainRenderPass;

protected:
    VkCommandPool                   _gfxCmdPool;
    std::vector<VkCommandBuffer>    _gfxCmdBuffers;
    uint32_t                        _gfxQueueIndex;
    uint32_t                        _prsQueueIndex;

protected:
    VkInstance          _instance;
    VkDevice            _device;
    VkQueue             _graphicsQueue;
};

struct VulkanRenderer
{
    LibHandle           vulkanLib;
    VkInstance          instance;
    VkSwapchainKHR      swapChain;
    VkPhysicalDevice    physicalDevice;
    VkDevice            device;

    VulkanRenderer();
};

void create_vulkan_renderer(VulkanRenderer& renderer, ReaperRoot& root, Window* window);
void destroy_vulkan_renderer(VulkanRenderer& renderer, ReaperRoot& root);

#endif // REAPER_SWAPCHAINRENDERERBASE_INCLUDED
