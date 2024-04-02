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

#include <profiling/Scope.h>

#include "neptune/Constants.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/gtx/quaternion.hpp>

#include <array>
#include <random>

using namespace Reaper;

namespace Neptune
{
constexpr float ThetaMax = 0.8f * Math::HalfPi;
constexpr float PhiMax = 1.0f * Math::Pi;
constexpr float RollMax = 0.25f * Math::Pi;
constexpr float WidthMin = 20.0f * MeterInGameUnits;
constexpr float WidthMax = 50.0f * MeterInGameUnits;

constexpr u32 MinLength = 3;
constexpr u32 MaxLength = 1000;
constexpr u32 MaxTryCount = 10000;

constexpr u32 SplineOrder = 3;

constexpr float SplineInnerWeight = 0.5f;

constexpr u32 BoneCountPerChunk = 2;

using RNG = std::mt19937;

using Math::UnitXAxis;
using Math::UnitYAxis;
using Math::UnitZAxis;

namespace
{
    struct ChunkEnd
    {
        glm::fmat4x3 transform;
        float        phi_angle;
        float        theta_angle;
        float        roll_angle;
    };

    ChunkEnd generate_chunk_end(const GenerationInfo& gen_info, RNG& rng)
    {
        glm::vec2 thetaBounds = glm::vec2(0.0f, ThetaMax) * gen_info.chaos;
        glm::vec2 phiBounds = glm::vec2(-PhiMax, PhiMax);
        glm::vec2 rollBounds = glm::vec2(-RollMax, RollMax) * gen_info.chaos;

        std::uniform_real_distribution<float> thetaDistribution(thetaBounds.x, thetaBounds.y);
        std::uniform_real_distribution<float> phiDistribution(phiBounds.x, phiBounds.y);
        std::uniform_real_distribution<float> rollDistribution(rollBounds.x, rollBounds.y);

        float theta = thetaDistribution(rng);
        float phi = phiDistribution(rng);
        float roll = rollDistribution(rng);

        glm::quat deviation = glm::angleAxis(phi, UnitXAxis) * glm::angleAxis(theta, UnitZAxis);
        glm::quat rollFixup = glm::angleAxis(-phi + roll, deviation * UnitXAxis);

        return ChunkEnd{
            .transform = glm::toMat4(rollFixup * deviation),
            .phi_angle = phi,
            .theta_angle = theta,
            .roll_angle = roll,
        };
    }

    bool is_node_self_colliding(std::span<const TrackSkeletonNode> nodes, const TrackSkeletonNode& current_node,
                                u32& outputNodeIdx)
    {
        for (u32 i = 0; (i + 1) < nodes.size(); i++)
        {
            const float distanceSq = glm::distance2(current_node.center_ws, nodes[i].center_ws);
            const float minRadius = current_node.radius + nodes[i].radius;

            if (distanceSq < (minRadius * minRadius))
            {
                outputNodeIdx = i;
                return true;
            }
        }

        return false;
    }

    TrackSkeletonNode generate_node(const GenerationInfo& gen_info, std::span<const TrackSkeletonNode> previous_nodes,
                                    RNG& rng)
    {
        std::uniform_real_distribution<float> widthDistribution(WidthMin, WidthMax);
        std::uniform_real_distribution<float> radiusDistribution(gen_info.radius_min_meter * MeterInGameUnits,
                                                                 gen_info.radius_max_meter * MeterInGameUnits);

        TrackSkeletonNode node;

        node.radius = radiusDistribution(rng);
        const glm::fvec3 forward_offset_ms = UnitXAxis * node.radius;

        if (previous_nodes.empty())
        {
            node.in_transform_ms_to_ws = glm::identity<glm::fmat4x3>();
            node.in_width = widthDistribution(rng);
            node.center_ws = forward_offset_ms;
        }
        else
        {
            const TrackSkeletonNode& previous_node = previous_nodes.back();

            node.in_transform_ms_to_ws = previous_node.out_transform_ms_to_ws;
            node.in_width = previous_node.out_width;
            node.center_ws = node.in_transform_ms_to_ws * glm::fvec4(forward_offset_ms, 1.f);
        }

        const ChunkEnd chunk_end = generate_chunk_end(gen_info, rng);
        node.end_transform = chunk_end.transform;
        node.phi_angle = chunk_end.phi_angle;
        node.theta_angle = chunk_end.theta_angle;
        node.roll_angle = chunk_end.roll_angle;

        const glm::fmat4 translation_a = glm::translate(glm::identity<glm::fmat4>(), forward_offset_ms);
        const glm::fmat4 translation_b =
            glm::translate(glm::identity<glm::fmat4>(), node.end_transform * glm::vec4(forward_offset_ms, 0.f));

        node.out_transform_ms_to_ws =
            node.in_transform_ms_to_ws * translation_a * translation_b * glm::fmat4(node.end_transform);
        node.out_width = widthDistribution(rng);

        node.in_transform_ws_to_ms = glm::inverse(glm::fmat4(node.in_transform_ms_to_ws));
        node.out_transform_ws_to_ms = glm::inverse(glm::fmat4(node.out_transform_ms_to_ws));

        return node;
    }
} // namespace

void generate_track_skeleton(const GenerationInfo& gen_info, std::span<TrackSkeletonNode> skeleton_nodes)
{
    Assert(gen_info.chunk_count >= MinLength);
    Assert(gen_info.chunk_count <= MaxLength);
    Assert(gen_info.chunk_count == skeleton_nodes.size());

    std::random_device rd;
    RNG                rng(rd());

    u32 tryCount = 0;
    u32 current_node_index = 0;

    while (current_node_index < gen_info.chunk_count && tryCount < MaxTryCount)
    {
        // Make span on generated nodes so far
        std::span<const TrackSkeletonNode> generated_nodes = std::span(skeleton_nodes.data(), current_node_index);

        const TrackSkeletonNode new_node = generate_node(gen_info, generated_nodes, rng);

        u32 collider_index;

        if (is_node_self_colliding(generated_nodes, new_node, collider_index))
        {
            current_node_index = collider_index + 1;
        }
        else
        {
            skeleton_nodes[current_node_index] = new_node;
            current_node_index += 1;
        }

        tryCount += 1;
    }

    Assert(tryCount < MaxTryCount, "something is majorly FUBAR");
}

namespace
{
    TrackSkinning generate_track_skinning_for_chunk(const TrackSkeletonNode& node)
    {
        TrackSkinning skinning;

        skinning.bind_pose_inv_transforms.resize(BoneCountPerChunk);

        // Compute inverse bind pose matrix
        for (u32 bone_index = 0; bone_index < BoneCountPerChunk; bone_index++)
        {
            const float     t = static_cast<float>(bone_index) / static_cast<float>(BoneCountPerChunk - 1);
            const float     param = t;
            const glm::vec3 offset = UnitXAxis * (param * node.radius * 2.f);

            // Inverse of a translation is the translation by the opposite vector
            skinning.bind_pose_inv_transforms[bone_index] = glm::translate(glm::mat4(1.0f), -offset);
        }

        // NOTE: local transform origin is the 'in' node, not the sphere center
        std::array<glm::vec3, BoneCountPerChunk> bone_root_positions_ms;

        bone_root_positions_ms[0] = glm::vec3(0.0f);
        // FIXME We need to fix positions to be placed on the arc of the trajectory.
        // bone_root_positions_ms[1] = UnitXAxis * node.radius;
        bone_root_positions_ms[1] =
            UnitXAxis * node.radius + node.end_transform * glm::fvec4(UnitXAxis * node.radius, 0.f); // FIXME

        // Compute static pose matrix
        skinning.pose_transforms.resize(BoneCountPerChunk);

        for (u32 bone_index = 0; bone_index < BoneCountPerChunk; bone_index++)
        {
            const float t = static_cast<float>(bone_index) / static_cast<float>(BoneCountPerChunk - 1);
            const float theta_angle = t * node.theta_angle;
            const float roll_angle = t * node.roll_angle;

            glm::quat deviation = glm::angleAxis(node.phi_angle, UnitXAxis) * glm::angleAxis(theta_angle, UnitZAxis);
            glm::quat rollFixup = glm::angleAxis(-node.phi_angle + roll_angle, deviation * UnitXAxis);

            glm::mat4x3 transform = glm::translate(glm::identity<glm::fmat4>(), bone_root_positions_ms[bone_index]);

            skinning.pose_transforms[bone_index] = transform * glm::toMat4(rollFixup * deviation);
        }

        return skinning;
    }
} // namespace

void generate_track_skinning(std::span<const TrackSkeletonNode> skeleton_nodes, std::span<TrackSkinning> skinning)
{
    Assert(skeleton_nodes.size() == skinning.size());

    for (u32 chunk_index = 0; chunk_index < skinning.size(); chunk_index++)
    {
        skinning[chunk_index] = generate_track_skinning_for_chunk(skeleton_nodes[chunk_index]);
    }
}

namespace
{
    std::array<float, BoneCountPerChunk> compute_track_bone_weights(float t)
    {
        Assert(t >= 0.f - Math::DefaultEpsilon);
        Assert(t <= 1.f + Math::DefaultEpsilon);

        float debug_sum = 0.f;

        std::array<float, BoneCountPerChunk> weights;

        for (u32 bone_index = 0; bone_index < BoneCountPerChunk; bone_index++)
        {
            float weight = std::max(0.f, 1.f - glm::abs(t * float(BoneCountPerChunk - 1) - float(bone_index)));
            weights[bone_index] = weight;

            debug_sum += weight;
        }

#if 1
        Assert(Math::IsEqualWithEpsilon(debug_sum, 1.0f));
#endif

        return weights;
    }
} // namespace

void skin_track_chunk_mesh(const TrackSkeletonNode& node, const TrackSkinning& track_skinning,
                           std::span<glm::fvec3> vertices, float mesh_length)
{
    REAPER_PROFILE_SCOPE_FUNC();

    const u32                            bone_count = BoneCountPerChunk;
    std::array<glm::fmat4x3, bone_count> bone_transforms;

    for (u32 bone_index = 0; bone_index < bone_count; bone_index++)
    {
        bone_transforms[bone_index] = track_skinning.pose_transforms[bone_index]
                                      * glm::fmat4(track_skinning.bind_pose_inv_transforms[bone_index]);
    }

    const u32 vertex_count = static_cast<u32>(vertices.size());
    Assert(vertex_count > 0);

    const float chunk_length = node.radius * 2.f;
    const float scaleX = chunk_length / mesh_length;

    for (u32 vertex_index = 0; vertex_index < vertex_count; vertex_index++)
    {
        const glm::fvec3                           vertex = vertices[vertex_index] * glm::fvec3(scaleX, 1.0f, 1.0f);
        const std::array<float, BoneCountPerChunk> bone_weights = compute_track_bone_weights(vertex.x / chunk_length);

        glm::fvec3 skinned_position(0.0f);
        float      weight_sum = 0.f;

        for (u32 bone_index = 0; bone_index < bone_count; bone_index++)
        {
            float weight = bone_weights[bone_index];

            skinned_position += (bone_transforms[bone_index] * glm::fvec4(vertex, 1.0f)) * weight;
            weight_sum += weight;
        }

        if (glm::abs(weight_sum) > 0.0f)
        {
            skinned_position /= weight_sum;
        }

        vertices[vertex_index] = skinned_position;
    }
}
} // namespace Neptune
