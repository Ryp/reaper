#pragma once

#include "FrameGraph.h"

#include "Core3D/RenderTextureViewProperties.h"

namespace Reaper
{
struct GPUTextureProperties;
struct TGPUTextureUsage;
class TBaseTextureEntity;
class TBaseRenderTextureViewEntity;
class TRenderTexture;
} // namespace Reaper

namespace Reaper
{
namespace FrameGraph
{
    struct FrameGraphContext
    {
        using TBaseTextureEntityVector = std::vector<refptr<TBaseTextureEntity>>;
        using TBaseTextureEntityViewVector = std::vector<TBaseRenderTextureViewEntity*>;

        FrameGraph Graph;

        std::vector<refptr<TRenderTexture>> RenderTextures;
    };

    using TTextureViewEntitiesMap = SMALLFASTMAP(MiscRender, TRenderTextureViewProperties,
                                                 std::unique_ptr<TBaseRenderTextureViewEntity>, 2);

    struct TTextureEntityAlloc
    {
        refptr<TBaseTextureEntity> Entity;
        TTextureViewEntitiesMap    ViewEntities;
        bool                       IsLocked;        // Thibault S. (09/02/2018) Useful during allocation
        u32                        AllocsThisFrame; // Aliasing counter, if this number is big we are golden
    };

    using TTexturePool = std::vector<TTextureEntityAlloc>;

    struct TEntityPool
    {
        TTexturePool TexturePool;
    };
} // namespace FrameGraph
} // namespace Reaper
