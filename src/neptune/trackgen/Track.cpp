////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Track.h"

#include <core/Assert.h>

#include <math/Constants.h>
#include <math/FloatComparison.h>
#include <math/Spline.h>

#include <mesh/Mesh.h>

#include "neptune/Constants.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/projection.hpp>

#include <random>

using namespace Reaper;

namespace Neptune
{
constexpr float ThetaMax = 0.8f * Math::HalfPi;
constexpr float PhiMax = 1.0f * Math::Pi;
constexpr float RollMax = 0.25f * Math::Pi;
constexpr float WidthMin = 20.0f * MeterInGameUnits;
constexpr float WidthMax = 50.0f * MeterInGameUnits;
constexpr float RadiusMin = 100.0f * MeterInGameUnits;
constexpr float RadiusMax = 300.0f * MeterInGameUnits;

constexpr u32 MinLength = 3;
constexpr u32 MaxLength = 1000;
constexpr u32 MaxTryCount = 10000;

constexpr u32 SplineOrder = 3;

constexpr float SplineInnerWeight = 0.5f;

constexpr u32 BoneCountPerChunk = 4;

using RNG = std::mt19937;

using Math::UnitXAxis;
using Math::UnitYAxis;
using Math::UnitZAxis;

namespace
{
    glm::quat GenerateChunkEndLocalSpace(const GenerationInfo& genInfo, RNG& rng)
    {
        const glm::vec2 thetaBounds = glm::vec2(0.0f, ThetaMax) * genInfo.chaos;
        const glm::vec2 phiBounds = glm::vec2(-PhiMax, PhiMax) * genInfo.chaos;
        const glm::vec2 rollBounds = glm::vec2(-RollMax, RollMax) * genInfo.chaos;

        std::uniform_real_distribution<float> thetaDistribution(thetaBounds.x, thetaBounds.y);
        std::uniform_real_distribution<float> phiDistribution(phiBounds.x, phiBounds.y);
        std::uniform_real_distribution<float> rollDistribution(rollBounds.x, rollBounds.y);

        const float theta = thetaDistribution(rng);
        const float phi = phiDistribution(rng);
        const float roll = rollDistribution(rng);

        glm::quat deviation = glm::angleAxis(phi, UnitXAxis) * glm::angleAxis(theta, UnitZAxis);
        glm::quat rollFixup = glm::angleAxis(-phi + roll, deviation * UnitXAxis);

        return rollFixup * deviation;
    }

    bool is_node_self_colliding(nonstd::span<const TrackSkeletonNode> nodes, const TrackSkeletonNode& currentNode,
                                u32& outputNodeIdx)
    {
        for (u32 i = 0; (i + 1) < nodes.size(); i++)
        {
            const float distanceSq = glm::distance2(currentNode.positionWS, nodes[i].positionWS);
            const float minRadius = currentNode.radius + nodes[i].radius;

            if (distanceSq < (minRadius * minRadius))
            {
                outputNodeIdx = i;
                return true;
            }
        }

        return false;
    }

    TrackSkeletonNode generate_node(const GenerationInfo& genInfo, nonstd::span<const TrackSkeletonNode> skeleton_nodes,
                                    RNG& rng)
    {
        std::uniform_real_distribution<float> widthDistribution(WidthMin, WidthMax);
        std::uniform_real_distribution<float> radiusDistribution(RadiusMin, RadiusMax);

        TrackSkeletonNode node;

        node.radius = radiusDistribution(rng);

        if (skeleton_nodes.empty())
        {
            node.orientation_ms_to_ws = glm::quat();
            node.inWidth = widthDistribution(rng);
            node.positionWS = glm::vec3(0.f);
        }
        else
        {
            const TrackSkeletonNode& previousNode = skeleton_nodes.back();

            node.orientation_ms_to_ws = previousNode.orientation_ms_to_ws * previousNode.end_orientation_ms;
            node.inWidth = previousNode.outWidth;

            const glm::vec3 offsetMS = UnitXAxis * (previousNode.radius + node.radius);
            const glm::vec3 offsetWS = node.orientation_ms_to_ws * offsetMS;

            node.positionWS = previousNode.positionWS + offsetWS;
        }

        node.end_orientation_ms = GenerateChunkEndLocalSpace(genInfo, rng);
        node.outWidth = widthDistribution(rng);

        return node;
    }
} // namespace

void generate_track_skeleton(const GenerationInfo& genInfo, nonstd::span<TrackSkeletonNode> skeleton_nodes)
{
    Assert(genInfo.length >= MinLength);
    Assert(genInfo.length <= MaxLength);
    Assert(genInfo.length == skeleton_nodes.size());

    std::random_device rd;
    RNG                rng(rd());

    u32 tryCount = 0;
    u32 current_node_index = 0;

    while (current_node_index < genInfo.length && tryCount < MaxTryCount)
    {
        // Make span on generated nodes so far
        nonstd::span<const TrackSkeletonNode> generated_nodes =
            nonstd::make_span(skeleton_nodes.data(), current_node_index);

        const TrackSkeletonNode new_node = generate_node(genInfo, generated_nodes, rng);

        u32 collider_index = 0;

        if (is_node_self_colliding(generated_nodes, new_node, collider_index))
        {
            current_node_index = collider_index + 1;
        }
        else
        {
            skeleton_nodes[current_node_index] = new_node;
            current_node_index += 1;
        }

        ++tryCount;
    }

    Assert(tryCount < MaxTryCount, "something is majorly FUBAR");
}

void generate_track_splines(nonstd::span<const TrackSkeletonNode> skeleton_nodes,
                            nonstd::span<Reaper::Math::Spline>    splines)
{
    Assert(skeleton_nodes.size() == splines.size());

    std::vector<glm::vec4> controlPoints(4);

    for (u32 chunk_index = 0; chunk_index < skeleton_nodes.size(); chunk_index++)
    {
        const TrackSkeletonNode& node = skeleton_nodes[chunk_index];

        // Laying down 1 control point at each end and 2 in the center of the sphere
        // seems to work pretty well.
        controlPoints[0] = glm::vec4(UnitXAxis * -node.radius, 1.0f);
        controlPoints[1] = glm::vec4(glm::vec3(0.0f), SplineInnerWeight);
        controlPoints[2] = glm::vec4(glm::vec3(0.0f), SplineInnerWeight);
        controlPoints[3] = glm::vec4(node.end_orientation_ms * UnitXAxis * node.radius, 1.0f);

        splines[chunk_index] = Math::create_spline(SplineOrder, controlPoints);
    }
}

namespace
{
    TrackSkinning generate_track_skinning_for_chunk(const TrackSkeletonNode& node, const Math::Spline& spline)
    {
        TrackSkinning skinningInfo;
        skinningInfo.bones.resize(BoneCountPerChunk);

        // Fill bone positions
        skinningInfo.bones[0].root = spline_eval(spline, 0.0f);
        skinningInfo.bones[BoneCountPerChunk - 1].end = spline_eval(spline, 1.0f);

        // Compute bone start and end positions
        for (u32 i = 1; i < BoneCountPerChunk; i++)
        {
            const float     param = static_cast<float>(i) / static_cast<float>(BoneCountPerChunk);
            const glm::vec3 anchorPos = spline_eval(spline, param);

            skinningInfo.bones[i - 1].end = anchorPos;
            skinningInfo.bones[i].root = anchorPos;
        }

        skinningInfo.invBindTransforms.resize(BoneCountPerChunk);

        // Compute inverse bind pose matrix
        for (u32 i = 0; i < BoneCountPerChunk; i++)
        {
            const float     t = static_cast<float>(i) / static_cast<float>(BoneCountPerChunk);
            const float     param = t * 2.0f - 1.0f;
            const glm::vec3 offset = UnitXAxis * (param * node.radius);

            // Inverse of a translation is the translation by the opposite vector
            skinningInfo.invBindTransforms[i] = glm::translate(glm::mat4(1.0f), -offset);
        }

        skinningInfo.poseTransforms.resize(BoneCountPerChunk);

        // Compute current pose matrix by reconstructing an ortho-base
        // We only have roll information at chunk boundary, so we have to
        // use tricks to get the correct bone rotation
        for (u32 i = 0; i < BoneCountPerChunk; i++)
        {
            const float      t = static_cast<float>(i) / static_cast<float>(BoneCountPerChunk);
            const glm::fquat interpolatedOrientation = glm::slerp(glm::quat(), node.end_orientation_ms, t);

            const Bone&      bone = skinningInfo.bones[i];
            const glm::fvec3 plusX = glm::normalize(bone.end - bone.root);
            const glm::fvec3 interpolatedPlusY = interpolatedOrientation * UnitYAxis;
            const glm::fvec3 plusY = glm::normalize(
                interpolatedPlusY
                - glm::proj(interpolatedPlusY, plusX)); // Trick to get a correct-ish roll along the spline
            const glm::fvec3 plusZ = glm::cross(plusX, plusY);

            Assert(Math::IsEqualWithEpsilon(glm::length2(plusZ), 1.0f));

            // Convert to a matrix
            const glm::fmat4 rotation = glm::fmat3(plusX, plusY, plusZ);
            const glm::fmat4 translation = glm::translate(glm::fmat4(1.0f), bone.root);

            skinningInfo.poseTransforms[i] = translation * rotation;
        }

        return skinningInfo;
    }
} // namespace

void generate_track_skinning(nonstd::span<const TrackSkeletonNode> skeleton_nodes,
                             nonstd::span<const Reaper::Math::Spline>
                                 splines,
                             nonstd::span<TrackSkinning>
                                 skinning)
{
    Assert(skeleton_nodes.size() == splines.size());
    Assert(splines.size() == skinning.size());

    for (u32 chunk_index = 0; chunk_index < splines.size(); chunk_index++)
    {
        skinning[chunk_index] = generate_track_skinning_for_chunk(skeleton_nodes[chunk_index], splines[chunk_index]);
    }
}

namespace
{
    glm::fvec4 ComputeBoneWeights(glm::vec3 position, float radius)
    {
        const float t = (position.x / radius) * 2.0f + 2.0f;
        return glm::clamp(glm::vec4(1.0f - glm::max(t - 0.5f, 0.0f),
                                    1.0f - glm::abs(t - 1.5f),
                                    1.0f - glm::abs(t - 2.5f),
                                    1.0f - glm::max(3.5f - t, 0.0f)),
                          0.0f, 1.0f);
    }
} // namespace

void skin_track_chunk_mesh(const TrackSkeletonNode& node, const TrackSkinning& track_skinning, Mesh& mesh,
                           float meshLength)
{
    const u32               vertex_count = static_cast<u32>(mesh.positions.size());
    const u32               bone_count = 4; // FIXME choose if this is a hard limit or not
    std::vector<glm::fvec3> skinnedVertices(vertex_count);

    Assert(vertex_count > 0);

    const float scaleX = node.radius / (meshLength * 0.5f);

    for (u32 i = 0; i < vertex_count; i++)
    {
        const glm::fvec3 vertex = mesh.positions[i] * glm::fvec3(scaleX, 1.0f, 1.0f);
        const glm::fvec4 boneWeights = ComputeBoneWeights(vertex, node.radius);

        const float debugSum = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
        Assert(Math::IsEqualWithEpsilon(debugSum, 1.0f));

        glm::fvec4 skinnedVertex(0.0f);
        for (u32 j = 0; j < bone_count; j++)
        {
            const glm::fmat4 boneTransform = track_skinning.poseTransforms[j] * track_skinning.invBindTransforms[j];

            skinnedVertex += (boneTransform * glm::fvec4(vertex, 1.0f)) * boneWeights[j];
        }
        if (glm::abs(skinnedVertex.w) > 0.0f)
            skinnedVertices[i] = glm::fvec3(skinnedVertex) / skinnedVertex.w;

        skinnedVertices[i] = node.orientation_ms_to_ws * skinnedVertices[i];
    }
    mesh.positions = skinnedVertices;
}
} // namespace Neptune
