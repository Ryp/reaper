////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Test.h"

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

namespace SplineSonic
{
PhysicsSim create_sim()
{
    PhysicsSim sim = {};

    // Physics tweakables
    sim.pLinearFriction = 4.0f;
    sim.pQuadFriction = 0.01f;
    sim.pHandling = 30.0f;
    sim.pBrakesForce = 30.0f;

    // Boilerplate code for a standard rigidbody simulation
    sim.broadphase = new btDbvtBroadphase();
    sim.collisionConfiguration = new btDefaultCollisionConfiguration();
    sim.dispatcher = new btCollisionDispatcher(sim.collisionConfiguration);
    btGImpactCollisionAlgorithm::registerAlgorithm(sim.dispatcher);
    sim.solver = new btSequentialImpulseConstraintSolver;

    // Putting all of that together
    sim.dynamicsWorld =
        new btDiscreteDynamicsWorld(sim.dispatcher, sim.broadphase, sim.solver, sim.collisionConfiguration);

    // FIXME is there a lifetime issue with this ?
    // auto preTickCallback = [](btDynamicsWorld* world, btScalar dt) {
    //     auto context = static_cast<BulletContext*>(world->getWorldUserInfo());
    //     context->internalBulletPreTick(dt);
    // };

    // // FIXME is there a lifetime issue with this ?
    // auto postTickCallback = [](btDynamicsWorld* world, btScalar dt) {
    //     auto context = static_cast<BulletContext*>(world->getWorldUserInfo());
    //     context->internalBulletPostTick(dt);
    // };

    // sim.dynamicsWorld->setInternalTickCallback(preTickCallback, static_cast<void*>(this), true);
    // sim.dynamicsWorld->setInternalTickCallback(postTickCallback, static_cast<void*>(this), false);

    return sim;
}

void destroy_sim(PhysicsSim& sim)
{
    for (auto rb : sim.rigidBodies)
    {
        sim.dynamicsWorld->removeRigidBody(rb);
        delete rb->getMotionState();
        delete rb->getCollisionShape();
        delete rb;
    }
    for (auto vb : sim.vertexArrayInterfaces)
        delete vb;

    // for (auto playerInfo : sim.players)
    // {
    //     sim.dynamicsWorld->removeRigidBody(playerInfo.body);
    //     delete playerInfo.body->getMotionState();
    //     delete playerInfo.body->getCollisionShape();
    //     delete playerInfo.body;
    // }
    // Delete the rest of the bullet context
    delete sim.dynamicsWorld;
    delete sim.solver;
    delete sim.dispatcher;
    delete sim.collisionConfiguration;
    delete sim.broadphase;
}
} // namespace SplineSonic
