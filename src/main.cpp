////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "core/memory/LinearAllocator.h"
#include "core/memory/StackAllocator.h"
#include "core/memory/CacheLine.h"

#include "renderer/Renderer.h"

#include "game/pathing/CostMap.h"

// static void test1()
// {
//     auto a = new Camera();
//     auto b = new Camera;
//     auto c = new Camera[5];
//
//     delete[] c;
//     delete b;
//     delete a;
// }

// static void test2()
// {
//     StackAllocator  sa(1000);
//     Camera*         a = nullptr;
//     Camera*         b = nullptr;
//
//     a = static_cast<Camera*>(sa.alloc(sizeof(Camera)));
//     std::cout << "alloc=(" << sizeof(Camera) << ")=" << a << std::endl;
//
//     auto mk = sa.getMarker();
//
//     b = static_cast<Camera*>(sa.alloc(sizeof(Camera)));
//     std::cout << "alloc=(" << sizeof(Camera) << ")=" << b << std::endl;
//
//     sa.freeToMarker(mk);
//
//     b = static_cast<Camera*>(sa.alloc(sizeof(Camera)));
//     std::cout << "alloc=(" << sizeof(Camera) << ")=" << b << std::endl;
// }

// static void test3()
// {
//     int costMapSize = 100;
//     CostMap map = createCostMap(costMapSize);
//
//     std::cout << "cost[0][0]=" << static_cast<u16>(map.costs[0]);
//
//     destroyCostMap(map);
// }

#include "game/WorldUpdater.h"
#include "game/entitydb/EntityDb.h"

static void game_test()
{
    EntityDb        db;
    WorldUpdater    wu(&db);

    db.load();
    wu.load();
    for (int i = 0; i < 2000; ++i)
        wu.updateModules(0.15f);
    wu.unload();
    db.unload();
}

#include "renderer/vulkan/PresentationSurface.h"
static void vulkan_test()
{
    Window  window;

    window.Create("Vulkan Test");

    AbstractRenderer* renderer = AbstractRenderer::createRenderer();
    {
        renderer->startup(&window);
        window.renderLoop(renderer);
        renderer->shutdown();
    }
    delete renderer;
}

#include "renderer/mesh/ModelLoader.h"
static void mesh_test()
{
    ModelLoader loader;
    MeshCache cache;

    loader.load("res/model/bunny.obj", cache);
    loader.load("res/model/quad.obj", cache);
    loader.load("res/model/ship.obj", cache);
}

#include "renderer/texture/TextureLoader.h"
static void texture_test()
{
    TextureLoader loader;
    TextureCache cache;

    loader.load("res/texture/bricks_diffuse.dds", cache);
    loader.load("res/texture/default.dds", cache);
    loader.load("res/texture/ship.dds", cache);
//     loader.load("res/texture/abrahams.dds", cache);
}

int main(int /*ac*/, char** /*av*/)
{
    Assert(cacheLineSize() == REAPER_CACHELINE_SIZE);

//     game_test();
//     vulkan_test();

    mesh_test();
    texture_test();
    return 0;
}
