////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "TrackGenExport.h"

#include <core/Types.h>
#include <profiling/Scope.h>

#include <nonstd/span.hpp>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Reaper::Math
{
struct Spline;
}

struct Mesh;

namespace Neptune
{
struct GenerationInfo
{
    u32   length;
    float width;
    float chaos;
};

struct TrackSkeletonNode
{
    glm::fmat4x3 in_transform_ms_to_ws; // Transform of the input frame placed on the tangent of the bounding sphere
    glm::fmat4x3 end_transform;

    float radius;
    float in_width;
    float out_width;

    // Extra
    glm::fvec3   center_ws;
    glm::fmat4x3 out_transform_ms_to_ws;

    // Matrix inverses
    glm::fmat4x3 in_transform_ws_to_ms;
    glm::fmat4x3 out_transform_ws_to_ms;
};

struct Bone
{
    glm::vec3 root;
    glm::vec3 end;
};

struct TrackSkinning
{
    std::vector<Bone>       bones;
    std::vector<glm::fmat4> invBindTransforms;
    std::vector<glm::fmat4> poseTransforms;
};

NEPTUNE_TRACKGEN_API
void generate_track_skeleton(const GenerationInfo& genInfo, nonstd::span<TrackSkeletonNode> skeleton_nodes);

NEPTUNE_TRACKGEN_API
void generate_track_splines(
    nonstd::span<const TrackSkeletonNode> skeleton_nodes, nonstd::span<Reaper::Math::Spline> splines);

NEPTUNE_TRACKGEN_API
void generate_track_skinning(nonstd::span<const TrackSkeletonNode> skeleton_nodes,
                             nonstd::span<const Reaper::Math::Spline>
                                 splines,
                             nonstd::span<TrackSkinning>
                                 skinning);

NEPTUNE_TRACKGEN_API
void skin_track_chunk_mesh(const TrackSkeletonNode& node,
                           const TrackSkinning&     track_skinning,
                           Mesh&                    mesh,
                           float                    meshLength);
} // namespace Neptune
