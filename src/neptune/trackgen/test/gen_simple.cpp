////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include <doctest/doctest.h>

#include <fstream>
#include <span>

#include "math/Spline.h"

#include "neptune/trackgen/Track.h"

using namespace Neptune;

TEST_CASE("Track generation")
{
    std::vector<TrackSkeletonNode>    skeletonNodes;
    std::vector<Reaper::Math::Spline> splinesMS;
    std::vector<TrackSkinning>        skinning;

    GenerationInfo gen_info = {};
    gen_info.length = 100;
    gen_info.chaos = 1.0f;

    SUBCASE("Generate skeleton")
    {
        skeletonNodes.resize(gen_info.length);

        generate_track_skeleton(gen_info, skeletonNodes);

        CHECK_EQ(skeletonNodes.size(), gen_info.length);

        SUBCASE("")
        {}
        SUBCASE("Generate splines")
        {
            splinesMS.resize(gen_info.length);

            generate_track_splines(skeletonNodes, splinesMS);

            SUBCASE("")
            {}
            SUBCASE("Generate bones")
            {
                skinning.resize(gen_info.length);

                generate_track_skinning(skeletonNodes, splinesMS, skinning);
            }
        }
    }
}

#include "mesh/ModelLoader.h"

TEST_CASE("Track mesh generation")
{
    std::vector<TrackSkeletonNode>    skeletonNodes;
    std::vector<Reaper::Math::Spline> splinesMS;
    std::vector<TrackSkinning>        skinning;

    GenerationInfo gen_info = {};
    gen_info.length = 10;
    gen_info.chaos = 1.0f;

    skeletonNodes.resize(gen_info.length);
    splinesMS.resize(gen_info.length);
    skinning.resize(gen_info.length);

    generate_track_skeleton(gen_info, skeletonNodes);
    generate_track_splines(skeletonNodes, splinesMS);
    generate_track_skinning(skeletonNodes, splinesMS, skinning);

    const std::string assetFile("res/model/track_chunk_simple.obj");

    std::ofstream             outFile("test_skinned_track.obj");
    std::vector<Reaper::Mesh> meshes;

    for (u32 i = 0; i < gen_info.length; i++)
    {
        Reaper::Mesh& mesh = meshes.emplace_back(Reaper::load_obj(assetFile));

        skin_track_chunk_mesh(skeletonNodes[i], skinning[i], mesh.positions, 10.0f);
    }

    save_obj(outFile, meshes);
}
