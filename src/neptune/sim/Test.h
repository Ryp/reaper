////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SimExport.h"

#include <core/Types.h>

#include <nonstd/span.hpp>

#include <glm/fwd.hpp>
#include <unordered_map>
#include <vector>

class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btRigidBody;
class btStridingMeshInterface;
class btCollisionShape;
class btScaledBvhTriangleMeshShape;
class btTriangleIndexVertexArray;

struct Mesh;

namespace Neptune
{
struct ShipStats
{
    float thrust;
    float braking;
};

struct ShipInput
{
    float brake;    // 0.0 not braking - 1.0 max
    float throttle; // 0.0 no trottle - 1.0 max
    float steer;    // 0.0 no steering - -1.0 max left - 1.0 max right
};

using StaticMeshColliderHandle = u32;

struct StaticMeshCollider
{
    std::vector<u32>        indices;
    std::vector<glm::fvec3> vertex_positions;

    btRigidBody*                  rigid_body;
    btTriangleIndexVertexArray*   mesh_interface;
    btScaledBvhTriangleMeshShape* scaled_mesh_shape;
};

struct TrackSkeletonNode;

struct PhysicsSim
{
    float linear_friction;
    float quadratic_friction;
    u32   alloc_number_fixme; // FIXME

    // This must always be set before calling any sim update
    struct FrameData
    {
        ShipInput                             input;
        nonstd::span<const TrackSkeletonNode> skeleton_nodes;
    } frame_data;

#if defined(REAPER_USE_BULLET_PHYSICS)
    btBroadphaseInterface*               broadphase;
    btDefaultCollisionConfiguration*     collisionConfiguration;
    btCollisionDispatcher*               dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld*             dynamicsWorld;

    std::vector<btRigidBody*> players;

    std::unordered_map<StaticMeshColliderHandle, StaticMeshCollider> static_mesh_colliders;
#endif
};

NEPTUNE_SIM_API PhysicsSim create_sim();
NEPTUNE_SIM_API void       destroy_sim(PhysicsSim& sim);

NEPTUNE_SIM_API void sim_start(PhysicsSim* sim);

NEPTUNE_SIM_API void sim_update(PhysicsSim& sim, nonstd::span<const TrackSkeletonNode> skeleton_nodes,
                                const ShipInput& input, float dt);
NEPTUNE_SIM_API glm::fmat4x3 get_player_transform(PhysicsSim& sim);

NEPTUNE_SIM_API
void sim_create_static_collision_meshes(nonstd::span<StaticMeshColliderHandle> handles, PhysicsSim& sim,
                                        nonstd::span<const Mesh>         meshes,
                                        nonstd::span<const glm::fmat4x3> transforms_no_scale,
                                        nonstd::span<const glm::fvec3>   scales = nonstd::span<const glm::fvec3>());

NEPTUNE_SIM_API
void sim_destroy_static_collision_meshes(nonstd::span<const StaticMeshColliderHandle> handles, PhysicsSim& sim);

NEPTUNE_SIM_API void sim_create_player_rigid_body(PhysicsSim& sim, const glm::fmat4x3& player_transform,
                                                  const glm::fvec3& shape_extent);
} // namespace Neptune
