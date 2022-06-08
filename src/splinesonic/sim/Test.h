////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SimExport.h"

#include <nonstd/span.hpp>

#include <glm/matrix.hpp>
#include <vector>

class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btRigidBody;
class btStridingMeshInterface;

struct Mesh;

namespace SplineSonic
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

struct PhysicsSim
{
    float     linear_friction;
    float     quadratic_friction;
    ShipInput last_input;

#if defined(REAPER_USE_BULLET_PHYSICS)
    btBroadphaseInterface*               broadphase;
    btDefaultCollisionConfiguration*     collisionConfiguration;
    btCollisionDispatcher*               dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld*             dynamicsWorld;

    std::vector<btRigidBody*>             static_meshes;
    std::vector<btRigidBody*>             players;
    std::vector<btStridingMeshInterface*> vertexArrayInterfaces;
#endif
};

SPLINESONIC_SIM_API PhysicsSim create_sim();
SPLINESONIC_SIM_API void       destroy_sim(PhysicsSim& sim);

SPLINESONIC_SIM_API void sim_start(PhysicsSim* sim);

SPLINESONIC_SIM_API void sim_update(PhysicsSim& sim, const ShipInput& input, float dt);
SPLINESONIC_SIM_API glm::fmat4x3 get_player_transform(PhysicsSim& sim);

SPLINESONIC_SIM_API void sim_register_static_collision_meshes(PhysicsSim& sim, const nonstd::span<Mesh> meshes,
                                                              const nonstd::span<glm::fmat4x3> transforms);
SPLINESONIC_SIM_API void sim_create_player_rigid_body(PhysicsSim& sim, const glm::fmat4x3& player_transform,
                                                      const glm::fvec3& shape_extent);
} // namespace SplineSonic
