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

#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace Reaper
{
namespace Math
{
    struct Spline;
}
} // namespace Reaper

namespace Neptune
{
constexpr float DefaultRadiusMinMeter = 100.0f;
constexpr float DefaultRadiusMaxMeter = 200.0f;

struct GenerationInfo
{
    u32   chunk_count = 10;
    float radius_min_meter = DefaultRadiusMinMeter;
    float radius_max_meter = DefaultRadiusMaxMeter;
    float chaos = 0.2f;
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
    std::vector<Bone>         bones;
    std::vector<glm::fmat4x3> invBindTransforms;
    std::vector<glm::fmat4x3> poseTransforms;
};

NEPTUNE_TRACKGEN_API
void generate_track_skeleton(const GenerationInfo& genInfo, std::span<TrackSkeletonNode> skeleton_nodes);

NEPTUNE_TRACKGEN_API
void generate_track_splines(std::span<const TrackSkeletonNode> skeleton_nodes, std::span<Reaper::Math::Spline> splines);

NEPTUNE_TRACKGEN_API
void generate_track_skinning(std::span<const TrackSkeletonNode> skeleton_nodes,
                             std::span<const Reaper::Math::Spline>
                                 splines,
                             std::span<TrackSkinning>
                                 skinning);

NEPTUNE_TRACKGEN_API
void skin_track_chunk_mesh(const TrackSkeletonNode& node,
                           const TrackSkinning&     track_skinning,
                           std::span<glm::fvec3>
                                 vertices,
                           float mesh_length);
} // namespace Neptune
