////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <btBulletDynamicsCommon.h>

#include <vector>

namespace SplineSonic
{
struct PhysicsSim
{
    float pLinearFriction;
    float pQuadFriction;
    float pHandling;
    float pBrakesForce;

    btBroadphaseInterface*               broadphase;
    btDefaultCollisionConfiguration*     collisionConfiguration;
    btCollisionDispatcher*               dispatcher;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld*             dynamicsWorld;

    float                                 simulationTime;
    std::vector<btRigidBody*>             rigidBodies;
    std::vector<btStridingMeshInterface*> vertexArrayInterfaces;
};

SPLINESONIC_SIM_API PhysicsSim create_sim();
SPLINESONIC_SIM_API void       destroy_sim(PhysicsSim& sim);
} // namespace SplineSonic
