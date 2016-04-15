////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "core/memory/LinearAllocator.h"
#include "core/memory/StackAllocator.h"
#include "core/memory/CacheLine.h"

#include "renderer/vulkan/VulkanRenderer.h"
#include "renderer/opengl/OpenGLRenderer.h"

#include "game/WorldUpdater.h"
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

// static void game_test()
// {
//     WorldUpdater    wu;
//
//     wu.load();
//     for (int i = 0; i < 100; ++i)
//         wu.updateModules(0.16f);
//
// //     test1();
// //     test2();
// //     test3();
// }


int main(int /*ac*/, char** /*av*/)
{
    Assert(cacheLineSize() == REAPER_CACHELINE_SIZE);

    VulkanRenderer::run();
    return 0;
}
