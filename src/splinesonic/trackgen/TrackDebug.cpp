////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TrackDebug.h"

#include <vector>

#include "Track.h"

namespace SplineSonic { namespace TrackGen
{
    namespace
    {
        struct Geometry
        {
            std::vector<glm::fvec3> vertices;
            std::vector<glm::fvec2> uvs;
            std::vector<glm::fvec3> normals;
            std::vector<glm::uvec3> indexes;
        };
    }

    void SaveTrackAsObj(std::ostream& output, TrackSkeleton& track)
    {
        output << "o Track" << std::endl;

        for (auto node : track.nodes)
        {
            output << 'v';
            output << ' ' << node.positionWS.x;
            output << ' ' << node.positionWS.y;
            output << ' ' << node.positionWS.z;
            output << std::endl;
        }

        for (u32 i = 1; i < track.nodes.size(); i++)
        {
            output << 'l';
            output << ' ' << i;
            output << ' ' << i + 1;
            output << std::endl;
        }
    }
}}
