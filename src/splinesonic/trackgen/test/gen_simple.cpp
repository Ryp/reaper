////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
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
            GenerateTrackSplines(testTrack.skeletonNodes, testTrack.splines);

            SUBCASE("") {}
            SUBCASE("Save splines as obj")
            {
                std::ofstream file2("test_splines.obj");
                SaveTrackSplinesAsObj(file2, testTrack.splines, 20);

                SUBCASE("Generate bones")
                {
                    GenerateTrackSkinning(testTrack.splines, testTrack.skinning);

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
}
