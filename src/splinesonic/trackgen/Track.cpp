////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Track.h"

#include "math/Constants.h"
#include "math/Spline.h"
#include "splinesonic/Constants.h"

#include <glm/gtx/norm.hpp>

#include <random>

namespace SplineSonic { namespace TrackGen
{
    constexpr float ThetaMax = 0.8f * Reaper::Math::HalfPi;
    constexpr float PhiMax = 1.0f * Reaper::Math::Pi;
    constexpr float RollMax = 0.25f * Reaper::Math::Pi;
    constexpr float WidthMin = 20.0f * MeterInGameUnits;
    constexpr float WidthMax = 50.0f * MeterInGameUnits;
    constexpr float RadiusMin = 100.0f * MeterInGameUnits;
    constexpr float RadiusMax = 300.0f * MeterInGameUnits;

    constexpr u32 MinLength = 5;
    constexpr u32 MaxLength = 1000;
    constexpr u32 MaxTryCount = 10000;

    constexpr u32 SplineOrder = 3;
    constexpr u32 SplineInnerWeight = 1.0f;

    using RNG = std::mt19937;

    using Reaper::Math::UnitXAxis;
    using Reaper::Math::UnitYAxis;
    using Reaper::Math::UnitZAxis;

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

        bool IsNodeSelfColliding(const std::vector<TrackSkeletonNode>& nodes, const TrackSkeletonNode& currentNode, u32& outputNodeIdx)
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

        TrackSkeletonNode GenerateNode(const GenerationInfo& genInfo, const std::vector<TrackSkeletonNode>& skeletonNodes, RNG& rng)
        {
            std::uniform_real_distribution<float> widthDistribution(WidthMin, WidthMax);
            std::uniform_real_distribution<float> radiusDistribution(RadiusMin, RadiusMax);

            TrackSkeletonNode node;

            node.radius = radiusDistribution(rng);

            if (skeletonNodes.empty())
            {
                node.inOrientation = glm::quat();
                node.inWidth = widthDistribution(rng);
                node.positionWS = glm::vec3(0.f);
            }
            else
            {
                const TrackSkeletonNode& previousNode = skeletonNodes.back();

                node.inOrientation = previousNode.outOrientation;
                node.inWidth = previousNode.outWidth;

                const glm::vec3 offsetMS = UnitXAxis * (previousNode.radius + node.radius);
                const glm::vec3 offsetWS = node.inOrientation * offsetMS;

                node.positionWS = previousNode.positionWS + offsetWS;
            }

            node.outOrientation = node.inOrientation * GenerateChunkEndLocalSpace(genInfo, rng);
            node.outWidth = widthDistribution(rng);

            return node;
        }
    }

    void GenerateTrackSkeleton(const GenerationInfo& genInfo, std::vector<TrackSkeletonNode>& skeletonNodes)
    {
        Assert(genInfo.length >= MinLength);
        Assert(genInfo.length <= MaxLength);

        skeletonNodes.resize(0);
        skeletonNodes.reserve(genInfo.length);

        std::random_device rd;
        RNG rng(rd());

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

    void GenerateTrackSplines(const std::vector<TrackSkeletonNode>& skeletonNodes, std::vector<Reaper::Math::Spline>& splines)
    {
        std::vector<glm::vec4> controlPoints(4);
        const u32 trackChunkCount = skeletonNodes.size();

        Assert(trackChunkCount > 0);
        Assert(trackChunkCount == skeletonNodes.size());

        splines.resize(trackChunkCount);

        for (u32 i = 0; i < trackChunkCount; i++)
        {
            const TrackSkeletonNode& node = skeletonNodes[i];

            controlPoints[0] = glm::vec4(node.positionWS - node.inOrientation * UnitXAxis * node.radius, 1.0f);
            controlPoints[1] = glm::vec4(node.positionWS, SplineInnerWeight);
            controlPoints[2] = glm::vec4(node.positionWS, SplineInnerWeight);
            controlPoints[3] = glm::vec4(node.positionWS + node.outOrientation * UnitXAxis * node.radius, 1.0f);

            splines[i] = Reaper::Math::ConstructSpline(SplineOrder, controlPoints);
        }
    }
}} // namespace SplineSonic::TrackGen

