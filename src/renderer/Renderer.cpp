////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Renderer.h"

#include "renderer/vulkan/VulkanRenderer.h"

AbstractRenderer* AbstractRenderer::createRenderer()
{
    return new VulkanRenderer();
}
