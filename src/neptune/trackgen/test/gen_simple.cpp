////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include <doctest/doctest.h>

#include <fstream>
#include <nonstd/span.hpp>

#include "math/Spline.h"

#include "neptune/trackgen/Track.h"
#include "neptune/trackgen/TrackDebug.h"

using namespace Neptune;

TEST_CASE("Track generation")
{
    std::vector<TrackSkeletonNode>    skeletonNodes;
    std::vector<Reaper::Math::Spline> splinesMS;
    std::vector<TrackSkinning>        skinning;

    GenerationInfo gen_info = {};
    gen_info.length = 100;
    gen_info.width = 12.0f;
    gen_info.chaos = 1.0f;

    SUBCASE("Generate skeleton")
    {
        skeletonNodes.resize(gen_info.length);

        generate_track_skeleton(gen_info, skeletonNodes);

        CHECK_EQ(skeletonNodes.size(), gen_info.length);

        SUBCASE("")
        {}
        SUBCASE("Save skeleton as obj")
        {
            std::ofstream file("test_skeleton.obj");
            write_track_skeleton_as_obj(file, skeletonNodes);
        }
        SUBCASE("Generate splines")
        {
            splinesMS.resize(gen_info.length);

            generate_track_splines(skeletonNodes, splinesMS);

            SUBCASE("")
            {}
            SUBCASE("Save splines as obj")
            {
                std::ofstream file2("test_splines.obj");
                write_track_splines_as_obj(file2, skeletonNodes, splinesMS, 20);
            }
            SUBCASE("Generate bones")
            {
                skinning.resize(gen_info.length);

                generate_track_skinning(skeletonNodes, splinesMS, skinning);

                SUBCASE("")
                {}
                SUBCASE("Save bones as obj")
                {
                    std::ofstream file3("test_bones.obj");
                    write_track_bones_as_obj(file3, skinning);
                }
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
    gen_info.width = 10.0f;
    gen_info.chaos = 1.0f;

    skeletonNodes.resize(gen_info.length);
    splinesMS.resize(gen_info.length);
    skinning.resize(gen_info.length);

    generate_track_skeleton(gen_info, skeletonNodes);
    generate_track_splines(skeletonNodes, splinesMS);
    generate_track_skinning(skeletonNodes, splinesMS, skinning);

    const std::string assetFile("res/model/track/chunk_simple.obj");

    std::ofstream     outFile("test_skinned_track.obj");
    std::vector<Mesh> meshes(gen_info.length);

    for (u32 i = 0; i < gen_info.length; i++)
    {
        std::ifstream file(assetFile);
        meshes[i] = ModelLoader::loadOBJ(file);

        skin_track_chunk_mesh(skeletonNodes[i], skinning[i], meshes[i], 10.0f);
    }

    SaveMeshesAsObj(outFile, meshes);
}
