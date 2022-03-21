////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Test.h"

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

#include "mesh/Mesh.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

inline glm::vec3 toGlm(btVector3 const& vec)
{
    return glm::vec3(vec.getX(), vec.getY(), vec.getZ());
}

inline glm::quat toGlm(btQuaternion const& quat)
{
    return glm::quat(quat.getW(), quat.getX(), quat.getY(), quat.getZ());
}

inline btVector3 toBt(glm::vec3 const& vec)
{
    return btVector3(vec.x, vec.y, vec.z);
}

inline btQuaternion toBt(glm::quat const& quat)
{
    return btQuaternion(quat.w, quat.x, quat.y, quat.z);
}

namespace glm
{
inline glm::vec3 forward()
{
    return glm::vec3(1.0f, 0.0f, 0.0f);
}
inline glm::vec3 up()
{
    return glm::vec3(0.0f, 1.0f, 0.0f);
}
inline glm::vec3 side()
{
    return glm::vec3(0.0f, 0.0f, 1.0f);
}
} // namespace glm

namespace SplineSonic
{
namespace
{
    static const int SimulationMaxSubStep = 3; // FIXME

    /*
    void steer(float dtSecs, Movement& movement, float amount)
    {
        float steerMultiplier = 0.035f;
        float inclinationMax = 2.6f;
        float inclForce = (log(glm::length(movement.speed * 0.5f) + 1.0f) + 0.2f) * amount;
        float inclinationRepel = -movement.inclination * 5.0f;

        movement.inclination =
            glm::clamp(movement.inclination + (inclForce + inclinationRepel) * dtSecs, -inclinationMax, inclinationMax);
        movement.orientation *= glm::angleAxis(-amount * steerMultiplier, glm::up());
    }
    */

    void pre_tick(PhysicsSim& sim, float dt)
    {
        static_cast<void>(sim);
        static_cast<void>(dt);

        /*
        // FIXME
        btRigidBody* playerRigidBody = sim.players[0].body;
        IPlayer*     player = sim.players[0].player;
        glm::vec3    forces;
        const auto&  factors = player->getMovementFactor();
        auto&        movement = player->getMovement();
        TrackChunk*  nearestChunk = sim.track->getNearestChunk(movement.position);
        auto         projection = nearestChunk->projectPosition(movement.position);
        glm::vec3    shipUp;
        glm::vec3    shipFwd;

        shipUp = projection.orientation * glm::up();
        shipFwd = movement.orientation * glm::forward();
        shipFwd = glm::normalize(shipFwd - glm::proj(shipFwd, shipUp)); // Ship Fwd in on slice plane

        // Re-eval ship orientation
        movement.orientation = glm::quat(glm::mat3(shipFwd, shipUp, glm::cross(shipFwd, shipUp)));

        // change steer angle
        steer(dt, movement, factors.getTurnFactor());

        // sum all forces
        // forces += - shipUp * 98.331f; // 10x earth gravity
        forces += -shipUp * 450.0f;                                                // lot of times earth
        gravity forces += shipFwd * factors.getThrustFactor() * player->getAcceleration(); // Engine thrust
        //     forces += shipFwd * player.getAcceleration(); // Engine thrust
        forces += -glm::proj(movement.speed, shipFwd) * factors.getBrakeFactor() * sim.pBrakesForce; // Brakes
        force forces += -glm::proj(movement.speed, glm::cross(shipFwd, shipUp)) * sim.pHandling;           //
        Handling term forces += -movement.speed * sim.pLinearFriction;                             // Linear
        friction term forces += -movement.speed * glm::length(movement.speed) * sim.pQuadFriction; //
        Quadratic friction term

        // Progressive repelling force that pushes the ship downwards
        float upSpeed = glm::length(movement.speed - glm::proj(movement.speed, shipUp));
        if (projection.relativePosition.y > 2.0f && upSpeed > 0.0f)
            forces += movement.orientation * (glm::vec3(0.0f, 2.0f - upSpeed, 0.0f) * 10.0f);

        playerRigidBody->clearForces();
        playerRigidBody->applyCentralForce(toBt(forces));
        */
    }

    void post_tick(PhysicsSim& sim, float /*dt*/)
    {
        btRigidBody* playerRigidBody = sim.players.front(); // FIXME
        toGlm(playerRigidBody->getLinearVelocity());
        toGlm(playerRigidBody->getOrientation());
    }
} // namespace

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

    return sim;
}

void destroy_sim(PhysicsSim& sim)
{
    for (auto rb : sim.static_meshes)
    {
        sim.dynamicsWorld->removeRigidBody(rb);
        delete rb->getMotionState();
        delete rb->getCollisionShape();
        delete rb;
    }

    for (auto vb : sim.vertexArrayInterfaces)
        delete vb;

    for (auto playerInfo : sim.players)
    {
        sim.dynamicsWorld->removeRigidBody(playerInfo);
        delete playerInfo->getMotionState();
        delete playerInfo->getCollisionShape();
        delete playerInfo;
    }

    // Delete the rest of the bullet context
    delete sim.dynamicsWorld;
    delete sim.solver;
    delete sim.dispatcher;
    delete sim.collisionConfiguration;
    delete sim.broadphase;
}

namespace
{
    void pre_tick_callback(btDynamicsWorld* world, btScalar dt)
    {
        auto sim_ptr = static_cast<PhysicsSim*>(world->getWorldUserInfo());
        Assert(sim_ptr);
        pre_tick(*sim_ptr, dt);
    }

    void post_tick_callback(btDynamicsWorld* world, btScalar dt)
    {
        auto sim_ptr = static_cast<PhysicsSim*>(world->getWorldUserInfo());
        Assert(sim_ptr);
        post_tick(*sim_ptr, dt);
    }
} // namespace

void sim_start(PhysicsSim* sim)
{
    Assert(sim);

    sim->dynamicsWorld->setInternalTickCallback(pre_tick_callback, sim, true);
    sim->dynamicsWorld->setInternalTickCallback(post_tick_callback, sim, false);
}

void sim_update(PhysicsSim& sim, float dt)
{
    sim.dynamicsWorld->stepSimulation(dt, SimulationMaxSubStep);
}

void sim_register_static_collision_meshes(PhysicsSim& sim, const nonstd::span<Mesh> meshes,
                                          const nonstd::span<glm::mat4> transforms)
{
    Assert(meshes.size() == transforms.size());

    for (u32 i = 0; i < meshes.size(); i++)
    {
        const Mesh& mesh = meshes[i];

        // Describe how the mesh is laid out in memory
        btIndexedMesh indexedMesh;
        indexedMesh.m_vertexType = PHY_FLOAT;
        indexedMesh.m_triangleIndexStride = sizeof(mesh.indexes[0]);
        indexedMesh.m_vertexStride = sizeof(mesh.positions[0]);
        indexedMesh.m_numTriangles = mesh.indexes.size() / 3;
        indexedMesh.m_numVertices = mesh.positions.size();
        indexedMesh.m_triangleIndexBase = reinterpret_cast<const u8*>(mesh.indexes.data());
        indexedMesh.m_vertexBase = reinterpret_cast<const u8*>(mesh.positions.data());

        // Create bullet collision shape based on the mesh described
        btTriangleIndexVertexArray* meshInterface = new btTriangleIndexVertexArray();
        meshInterface->addIndexedMesh(indexedMesh);
        btBvhTriangleMeshShape* meshShape = new btBvhTriangleMeshShape(meshInterface, true);

        // Create bullet default motionstate (the object is static here)
        // to set the correct position of the collision shape

        // toBt(chunk->getInfo().sphere.position)); FIXME
        btDefaultMotionState* chunkMotionState = new btDefaultMotionState(btTransform(toBt(transforms[i])));

        // Create the rigidbody with the collision shape
        btRigidBody::btRigidBodyConstructionInfo staticMeshRigidBodyInfo(0, chunkMotionState, meshShape);
        btRigidBody*                             staticRigidMesh = new btRigidBody(staticMeshRigidBodyInfo);

        // Add it to our scene
        sim.dynamicsWorld->addRigidBody(staticRigidMesh);

        // Keep track of bullet data
        sim.static_meshes.push_back(staticRigidMesh);
        sim.vertexArrayInterfaces.push_back(meshInterface);
    }
}

void sim_create_player_rigid_body(PhysicsSim& sim)
{
    btMotionState*    motionState = new btDefaultMotionState;
    btCollisionShape* collisionShape = new btCapsuleShapeX(2.0f, 3.0f);
    btScalar          mass = 10.f; // FIXME
    btVector3         inertia(0.f, 0.f, 0.f);

    collisionShape->calculateLocalInertia(mass, inertia);
    btRigidBody::btRigidBodyConstructionInfo constructInfo(mass, motionState, collisionShape, inertia);

    btRigidBody* playerInfo = new btRigidBody(constructInfo);
    playerInfo->setFriction(0.1f);
    playerInfo->setRollingFriction(0.8f);

    sim.dynamicsWorld->addRigidBody(playerInfo);
    sim.players.push_back(playerInfo);
}
} // namespace SplineSonic
