////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include <fstream>

#include "math/Spline.h"

#include "splinesonic/trackgen/Track.h"
#include "splinesonic/trackgen/TrackDebug.h"

using namespace SplineSonic::TrackGen;

TEST_CASE("Track generation")
{
    Track testTrack;

    GenerationInfo genInfo = {};
    genInfo.length = 100;
    genInfo.width = 12.0f;
    genInfo.chaos = 1.0f;

    testTrack.genInfo = genInfo;

    SUBCASE("Generate skeleton")
    {
        GenerateTrackSkeleton(genInfo, testTrack.skeletonNodes);

        CHECK_EQ(testTrack.skeletonNodes.size(), testTrack.genInfo.length);

        SUBCASE("") {}
        SUBCASE("Save skeleton as obj")
        {
            std::ofstream file("test_skeleton.obj");
            SaveTrackSkeletonAsObj(file, testTrack.skeletonNodes);
        }
        SUBCASE("Generate splines")
        {
            GenerateTrackSplines(testTrack.skeletonNodes, testTrack.splinesMS);

            SUBCASE("") {}
            SUBCASE("Save splines as obj")
            {
                std::ofstream file2("test_splines.obj");
                SaveTrackSplinesAsObj(file2, testTrack.skeletonNodes, testTrack.splinesMS, 20);
            }
            SUBCASE("Generate bones")
            {
                GenerateTrackSkinning(testTrack.skeletonNodes, testTrack.splinesMS, testTrack.skinning);

                SUBCASE("") {}
                SUBCASE("Save bones as obj")
                {
                    std::ofstream file3("test_bones.obj");
                    SaveTrackBonesAsObj(file3, testTrack.skinning);
                }
            }
        }
    }
}

#include "mesh/ModelLoader.h"

TEST_CASE("Track mesh generation")
{
    Track testTrack;

    GenerationInfo genInfo = {};
    genInfo.length = 10;
    genInfo.width = 10.0f;
    genInfo.chaos = 1.0f;

    testTrack.genInfo = genInfo;

    GenerateTrackSkeleton(genInfo, testTrack.skeletonNodes);
    GenerateTrackSplines(testTrack.skeletonNodes, testTrack.splinesMS);
    GenerateTrackSkinning(testTrack.skeletonNodes, testTrack.splinesMS, testTrack.skinning);

    const std::string assetFile("res/model/track/chunk_simple.obj");

    std::ofstream     outFile("test_skinned_track.obj");
    std::vector<Mesh> meshes(genInfo.length);

    for (u32 i = 0; i < genInfo.length; i++)
    {
        std::ifstream file(assetFile);
        meshes[i] = ModelLoader::loadOBJ(file);

        SkinTrackChunkMesh(testTrack.skeletonNodes[i], testTrack.skinning[i], meshes[i], 10.0f);
    }

    SaveMeshesAsObj(outFile, &meshes[0], static_cast<u32>(meshes.size()));
}
