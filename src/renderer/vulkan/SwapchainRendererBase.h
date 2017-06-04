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

    virtual void startup(IWindow* window) override;
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

struct PresentationInfo
{
    VkSurfaceKHR                surface;
    VkSurfaceCapabilitiesKHR    surfaceCaps;
    VkSurfaceFormatKHR          surfaceFormat;
    VkSwapchainKHR              swapchain;
    VkExtent2D                  surfaceExtent;
    VkSemaphore                 imageAvailableSemaphore;
    VkSemaphore                 renderingFinishedSemaphore;
    u32                         imageCount;
    std::vector<VkImage>        images;
    std::vector<VkImageView>    imageViews;
    std::vector<VkFramebuffer>  framebuffers;
    VkRenderPass                renderPass;

    PresentationInfo();
};

struct PhysicalDeviceInfo
{
    uint32_t            graphicsQueueIndex;
    uint32_t            presentQueueIndex;
};

struct DeviceInfo
{
    VkQueue  graphicsQueue;
    VkQueue  presentQueue;
};

struct REAPER_RENDERER_API VulkanBackend
{
    LibHandle           vulkanLib;
    VkInstance          instance;

    VkPhysicalDevice    physicalDevice;
    PhysicalDeviceInfo  physicalDeviceInfo;

    VkDevice            device;
    DeviceInfo          deviceInfo;

    PresentationInfo    presentInfo;

    VkDebugReportCallbackEXT debugCallback;

    VulkanBackend();
};

void create_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& renderer);
void destroy_vulkan_renderer_backend(ReaperRoot& root, VulkanBackend& renderer);

#endif // REAPER_SWAPCHAINRENDERERBASE_INCLUDED
