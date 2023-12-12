////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SimExport.h"

#include <core/Types.h>

#include <span>

#include <glm/fwd.hpp>
#include <glm/vec3.hpp>
#include <unordered_map>
#include <vector>

#include "neptune/trackgen/Track.h"

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
    float handling;
};

using StaticMeshColliderHandle = u32;

struct StaticMeshCollider
{
    std::vector<u32>       indices;
    std::vector<glm::vec3> vertex_positions;

    btRigidBody*                  rigid_body;
    btTriangleIndexVertexArray*   mesh_interface;
    btScaledBvhTriangleMeshShape* scaled_mesh_shape;
};

struct ShipInput
{
    float brake;    // 0.0 not braking - 1.0 max
    float throttle; // 0.0 no trottle - 1.0 max
    float steer;    // 0.0 no steering - -1.0 max left - 1.0 max right
};

struct RaycastSuspension
{
    glm::vec3 position_start_ms; // This shouldn't change over time
    glm::vec3 position_end_ms;   // This shouldn't change over time

    float length_max;
    float length_ratio_rest;
    float length_ratio_last;

    // Could be two staged or following a curve
    float spring_stiffness;

    float damper_friction_compression;
    float damper_friction_extension;

    glm::vec3 position_start_ws;
    glm::vec3 position_end_ws;
};

struct PhysicsSim
{
    struct Consts
    {
        static constexpr glm::fvec3 player_shape_half_extent = glm::fvec3(0.4f, 0.2f, 0.35f);
        static constexpr float      ship_mass = 2.f;
    } consts;

    struct Vars
    {
        float     simulation_substep_duration;
        int       max_simulation_substep_count;
        float     gravity_force_intensity;
        float     linear_friction;
        float     quadratic_friction;
        float     angular_friction;
        bool      enable_suspension_forces;
        float     max_suspension_force;
        float     default_spring_stiffness;
        float     default_damper_friction_compression;
        float     default_damper_friction_extension;
        bool      enable_debug_geometry;
        ShipStats default_ship_stats;
    } vars;

    u32 alloc_number_fixme; // FIXME

    // This must always be set before calling any sim update
    struct FrameData
    {
        ShipInput                          input;
        std::span<const TrackSkeletonNode> skeleton_nodes;
    } frame_data;

    std::vector<RaycastSuspension> raycast_suspensions;
    glm::fmat3x3                   last_gravity_frame;

#if defined(REAPER_USE_BULLET_PHYSICS)
    btBroadphaseInterface*               broadphase;
    btDefaultCollisionConfiguration*     collisionConfiguration;
    btCollisionDispatcher*               dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld*             dynamics_world;

    std::vector<btRigidBody*> players;

    std::unordered_map<StaticMeshColliderHandle, StaticMeshCollider> static_mesh_colliders;
#endif
};

NEPTUNE_SIM_API PhysicsSim create_sim();
NEPTUNE_SIM_API void       destroy_sim(PhysicsSim& sim);

NEPTUNE_SIM_API glm::fmat4x3 get_player_transform(PhysicsSim& sim);

NEPTUNE_SIM_API
void sim_create_static_collision_meshes(std::span<StaticMeshColliderHandle> handles, PhysicsSim& sim,
                                        std::span<const Mesh> meshes, std::span<const glm::fmat4x3> transforms_no_scale,
                                        std::span<const glm::fvec3> scales = std::span<const glm::fvec3>());

NEPTUNE_SIM_API
void sim_destroy_static_collision_meshes(std::span<const StaticMeshColliderHandle> handles, PhysicsSim& sim);

NEPTUNE_SIM_API void sim_create_player_rigid_body(PhysicsSim& sim, const glm::fmat4x3& player_transform);

inline glm::fvec3 forward()
{
    return glm::fvec3(1.0f, 0.0f, 0.0f);
}

inline glm::fvec3 up()
{
    return glm::fvec3(0.0f, 1.0f, 0.0f);
}
} // namespace Neptune
