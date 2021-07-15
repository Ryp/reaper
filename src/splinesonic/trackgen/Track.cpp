////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Track.h"

#include "math/Constants.h"
#include "math/FloatComparison.h"
#include "math/Spline.h"

#include "mesh/Mesh.h"

#include "splinesonic/Constants.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/projection.hpp>

#include <random>

using namespace Reaper;

namespace SplineSonic::TrackGen
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

    bool IsNodeSelfColliding(const std::vector<TrackSkeletonNode>& nodes, const TrackSkeletonNode& currentNode,
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

    TrackSkeletonNode GenerateNode(const GenerationInfo& genInfo, const std::vector<TrackSkeletonNode>& skeletonNodes,
                                   RNG& rng)
    {
        std::uniform_real_distribution<float> widthDistribution(WidthMin, WidthMax);
        std::uniform_real_distribution<float> radiusDistribution(RadiusMin, RadiusMax);

        TrackSkeletonNode node;

        node.radius = radiusDistribution(rng);

        if (skeletonNodes.empty())
        {
            node.orientationWS = glm::quat();
            node.inWidth = widthDistribution(rng);
            node.positionWS = glm::vec3(0.f);
        }
        else
        {
            const TrackSkeletonNode& previousNode = skeletonNodes.back();

            node.orientationWS = previousNode.orientationWS * previousNode.rotationLS;
            node.inWidth = previousNode.outWidth;

            const glm::vec3 offsetMS = UnitXAxis * (previousNode.radius + node.radius);
            const glm::vec3 offsetWS = node.orientationWS * offsetMS;

            node.positionWS = previousNode.positionWS + offsetWS;
        }

        node.rotationLS = GenerateChunkEndLocalSpace(genInfo, rng);
        node.outWidth = widthDistribution(rng);

        return node;
    }
} // namespace

void GenerateTrackSkeleton(const GenerationInfo& genInfo, std::vector<TrackSkeletonNode>& skeletonNodes)
{
    Assert(genInfo.length >= MinLength);
    Assert(genInfo.length <= MaxLength);

    skeletonNodes.resize(0);
    skeletonNodes.reserve(genInfo.length);

    std::random_device rd;
    RNG                rng(rd());

    u32 tryCount = 0;

    while (skeletonNodes.size() < genInfo.length && tryCount < MaxTryCount)
    {
        const TrackSkeletonNode node = GenerateNode(genInfo, skeletonNodes, rng);

        u32 colliderIdx = 0;
        if (IsNodeSelfColliding(skeletonNodes, node, colliderIdx))
            skeletonNodes.resize(colliderIdx + 1); // Shrink the vector and generate from collider
        else
            skeletonNodes.push_back(node);

        ++tryCount;
    }

    Assert(tryCount < MaxTryCount, "something is majorly FUBAR");
}

void GenerateTrackSplines(const std::vector<TrackSkeletonNode>& skeletonNodes, std::vector<Math::Spline>& splines)
{
    std::vector<glm::vec4> controlPoints(4);
    const u32              trackChunkCount = static_cast<u32>(skeletonNodes.size());

    Assert(trackChunkCount > 0);
    Assert(trackChunkCount == skeletonNodes.size());

    splines.resize(trackChunkCount);

    for (u32 i = 0; i < trackChunkCount; i++)
    {
        const TrackSkeletonNode& node = skeletonNodes[i];

        // Laying down 1 control point at each end and 2 in the center of the sphere
        // seems to work pretty well.
        controlPoints[0] = glm::vec4(UnitXAxis * -node.radius, 1.0f);
        controlPoints[1] = glm::vec4(glm::vec3(0.0f), SplineInnerWeight);
        controlPoints[2] = glm::vec4(glm::vec3(0.0f), SplineInnerWeight);
        controlPoints[3] = glm::vec4(node.rotationLS * UnitXAxis * node.radius, 1.0f);

        splines[i] = Math::ConstructSpline(SplineOrder, controlPoints);
    }
}

namespace
{
    void GenerateTrackSkinningForChunk(const TrackSkeletonNode& node,
                                       const Math::Spline&      spline,
                                       TrackSkinning&           skinningInfo)
    {
        skinningInfo.bones.resize(BoneCountPerChunk);

        // Fill bone positions
        skinningInfo.bones[0].root = EvalSpline(spline, 0.0f);
        skinningInfo.bones[BoneCountPerChunk - 1].end = EvalSpline(spline, 1.0f);

        // Compute bone start and end positions
        for (u32 i = 1; i < BoneCountPerChunk; i++)
        {
            const float     param = static_cast<float>(i) / static_cast<float>(BoneCountPerChunk);
            const glm::vec3 anchorPos = EvalSpline(spline, param);

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
            const glm::fquat interpolatedOrientation = glm::slerp(glm::quat(), node.rotationLS, t);

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
    }
} // namespace

void GenerateTrackSkinning(const std::vector<TrackSkeletonNode>& skeletonNodes,
                           const std::vector<Math::Spline>&      splines,
                           std::vector<TrackSkinning>&           skinning)
{
    const u32 trackChunkCount = static_cast<u32>(splines.size());

    Assert(trackChunkCount > 0);

    skinning.resize(trackChunkCount);

    for (u32 chunkIdx = 0; chunkIdx < splines.size(); chunkIdx++)
    {
        GenerateTrackSkinningForChunk(skeletonNodes[chunkIdx], splines[chunkIdx], skinning[chunkIdx]);
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

void SkinTrackChunkMesh(const TrackSkeletonNode& node, const TrackSkinning& trackSkinning, Mesh& mesh, float meshLength)
{
    const u32               vertexCount = static_cast<u32>(mesh.vertices.size());
    const u32               boneCount = 4; // FIXME choose if this is a hard limit or not
    std::vector<glm::fvec3> skinnedVertices(vertexCount);

    Assert(vertexCount > 0);

    const float scaleX = node.radius / (meshLength * 0.5f);

    for (u32 i = 0; i < vertexCount; i++)
    {
        const glm::fvec3 vertex = mesh.vertices[i] * glm::fvec3(scaleX, 1.0f, 1.0f);
        const glm::fvec4 boneWeights = ComputeBoneWeights(vertex, node.radius);

        const float debugSum = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
        Assert(Math::IsEqualWithEpsilon(debugSum, 1.0f));

        glm::fvec4 skinnedVertex(0.0f);
        for (u32 j = 0; j < boneCount; j++)
        {
            const glm::fmat4 boneTransform = trackSkinning.poseTransforms[j] * trackSkinning.invBindTransforms[j];

            skinnedVertex += (boneTransform * glm::fvec4(vertex, 1.0f)) * boneWeights[j];
        }
        if (glm::abs(skinnedVertex.w) > 0.0f)
            skinnedVertices[i] = glm::fvec3(skinnedVertex) / skinnedVertex.w;

        skinnedVertices[i] = node.orientationWS * skinnedVertices[i];
    }
    mesh.vertices = skinnedVertices;
}
} // namespace SplineSonic::TrackGen
