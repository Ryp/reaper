////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

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
struct PhysicsSim
{
    float pLinearFriction;
    float pQuadFriction;
    float pHandling;
    float pBrakesForce;

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

SPLINESONIC_SIM_API void sim_update(PhysicsSim& sim, float dt);

SPLINESONIC_SIM_API void sim_register_static_collision_meshes(PhysicsSim& sim, const nonstd::span<Mesh> meshes,
                                                              const nonstd::span<glm::mat4> transforms);
SPLINESONIC_SIM_API void sim_create_player_rigid_body(PhysicsSim& sim);
} // namespace SplineSonic
