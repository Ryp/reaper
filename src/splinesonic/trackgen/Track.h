////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Reaper::Math
{
struct Spline;
}

struct Mesh;

namespace SplineSonic
{
struct GenerationInfo
{
    u32   length;
    float width;
    float chaos;
};

struct TrackSkeletonNode
{
    glm::vec3 positionWS;
    float     radius;
    glm::quat orientationWS;
    glm::quat rotationLS; // Deviation of the chunk in local space
    float     inWidth;
    float     outWidth;
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

struct Track
{
    GenerationInfo                    genInfo;
    std::vector<TrackSkeletonNode>    skeletonNodes;
    std::vector<Reaper::Math::Spline> splinesMS;
    std::vector<TrackSkinning>        skinning;
};

SPLINESONIC_TRACKGEN_API
void generate_track_skeleton(const GenerationInfo& genInfo, std::vector<TrackSkeletonNode>& skeletonNodes);

SPLINESONIC_TRACKGEN_API
void generate_track_splines(const std::vector<TrackSkeletonNode>& skeletonNodes,
                            std::vector<Reaper::Math::Spline>&    splines);

SPLINESONIC_TRACKGEN_API
void generate_track_skinning(const std::vector<TrackSkeletonNode>&    skeletonNodes,
                             const std::vector<Reaper::Math::Spline>& splines,
                             std::vector<TrackSkinning>&              skinning);

SPLINESONIC_TRACKGEN_API
void skin_track_chunk_mesh(const TrackSkeletonNode& node,
                           const TrackSkinning&     trackSkinning,
                           Mesh&                    mesh,
                           float                    meshLength);
} // namespace SplineSonic
