////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Track.h"

#include <glm/gtx/norm.hpp>

#include <random>

#include "math/Constants.h"
#include "splinesonic/Constants.h"

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

        bool IsNodeSelfColliding(const TrackSkeletonNodeArray& nodes, const TrackSkeletonNode& currentNode, u32& outputNodeIdx)
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

        TrackSkeletonNode GenerateNode(const TrackSkeleton& track, RNG& rng)
        {
            std::uniform_real_distribution<float> widthDistribution(WidthMin, WidthMax);
            std::uniform_real_distribution<float> radiusDistribution(RadiusMin, RadiusMax);

            TrackSkeletonNode node;

            node.radius = radiusDistribution(rng);

            if (track.nodes.empty())
            {
                node.inOrientation = glm::quat();
                node.inWidth = widthDistribution(rng);
                node.positionWS = glm::vec3(0.f);
            }
            else
            {
                const TrackSkeletonNode previousNode = track.nodes.back();

                node.inOrientation = previousNode.outOrientation;
                node.inWidth = previousNode.outWidth;

                const glm::vec3 offsetMS = UnitXAxis * (previousNode.radius + node.radius);
                const glm::vec3 offsetWS = node.inOrientation * offsetMS;

                node.positionWS = previousNode.positionWS + offsetWS;
            }

            node.outOrientation = node.inOrientation * GenerateChunkEndLocalSpace(track.genInfo, rng);
            node.outWidth = widthDistribution(rng);

            return node;
        }
    }

    void GenerateTrackSkeleton(const GenerationInfo& genInfo, TrackSkeleton& track)
    {
        Assert(genInfo.length >= MinLength);
        Assert(genInfo.length <= MaxLength);

        TrackSkeletonNodeArray& nodes = track.nodes;
        nodes.resize(0);
        nodes.reserve(genInfo.length);

        track.genInfo = genInfo;

        std::random_device rd;
        RNG rng(rd());

        u32 tryCount = 0;

        while (nodes.size() < genInfo.length && tryCount < MaxTryCount)
        {
            const TrackSkeletonNode node = GenerateNode(track, rng);

            u32 colliderIdx = 0;
            if (IsNodeSelfColliding(nodes, node, colliderIdx))
                nodes.resize(colliderIdx + 1); // Shrink the vector and generate from collider
            else
                nodes.push_back(node);

            ++tryCount;
        }

        Assert(tryCount < MaxTryCount, "something is majorly FUBAR");
    }
}}

