////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// #include "game/pathing/CostMap.h"
// static void test3()
// {
//     int costMapSize = 100;
//     CostMap map = createCostMap(costMapSize);
//
//     std::cout << "cost[0][0]=" << static_cast<u16>(map.costs[0]);
//
//     destroyCostMap(map);
// }

//#include "game/WorldUpdater.h"
//#include "game/entitydb/EntityDb.h"
//static void game_test()
//{
//    EntityDb        db;
//    WorldUpdater    wu(&db);
//
//    db.load();
//    wu.load();
//    for (int i = 0; i < 2000; ++i)
//        wu.updateModules(0.15f);
//    wu.unload();
//    db.unload();
//}

#include "renderer/Renderer.h"
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

//#include "renderer/mesh/ModelLoader.h"
//static void mesh_test()
//{
//    ModelLoader loader;
//    MeshCache cache;
//
//    loader.load("res/model/bunny.obj", cache);
//    loader.load("res/model/quad.obj", cache);
//    loader.load("res/model/ship.obj", cache);
//}

//#include "renderer/texture/TextureLoader.h"
//static void texture_test()
//{
//    TextureLoader loader;
//    TextureCache cache;
//
//    loader.load("res/texture/bricks_diffuse.dds", cache);
//    loader.load("res/texture/default.dds", cache);
//    loader.load("res/texture/ship.dds", cache);
////     loader.load("res/texture/abrahams.dds", cache);
//}

int main(int /*ac*/, char** /*av*/)
{
//     game_test();
    vulkan_test();
    return 0;
}
