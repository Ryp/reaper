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
    std::vector<TrackSkeletonNode> skeletonNodes;
    std::vector<TrackSkinning>     skinning;

    GenerationInfo gen_info;

    SUBCASE("Generate skeleton")
    {
        skeletonNodes.resize(gen_info.chunk_count);

        generate_track_skeleton(gen_info, skeletonNodes);

        CHECK_EQ(skeletonNodes.size(), gen_info.chunk_count);

        SUBCASE("")
        {}
        SUBCASE("Generate bones")
        {
            skinning.resize(gen_info.chunk_count);

            generate_track_skinning(skeletonNodes, skinning);
        }
    }
}

#include "mesh/ModelLoader.h"

TEST_CASE("Track mesh generation")
{
    std::vector<TrackSkeletonNode> skeletonNodes;
    std::vector<TrackSkinning>     skinning;

    GenerationInfo gen_info;

    skeletonNodes.resize(gen_info.chunk_count);
    skinning.resize(gen_info.chunk_count);

    generate_track_skeleton(gen_info, skeletonNodes);
    generate_track_skinning(skeletonNodes, skinning);

    const std::string assetFile("res/model/track_chunk_simple.obj");

    std::ofstream             outFile("test_skinned_track.obj");
    std::vector<Reaper::Mesh> meshes;

    for (u32 i = 0; i < gen_info.chunk_count; i++)
    {
        Reaper::Mesh& mesh = meshes.emplace_back(Reaper::load_obj(assetFile));

        skin_track_chunk_mesh(skeletonNodes[i], skinning[i], mesh.positions, 10.0f);
    }

    save_obj(outFile, meshes);
}
