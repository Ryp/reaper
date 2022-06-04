////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Test.h"

#if defined(REAPER_USE_BULLET_PHYSICS)
#    include <BulletCollision/CollisionShapes/btBoxShape.h>
#    include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#    include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#    include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#    include <btBulletDynamicsCommon.h>
#endif

#include "mesh/Mesh.h"

#include "core/Assert.h"
#include "core/Profile.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <bit>

namespace SplineSonic
{
namespace
{
#if defined(REAPER_USE_BULLET_PHYSICS)
    inline glm::vec3 toGlm(btVector3 const& vec) { return glm::vec3(vec.getX(), vec.getY(), vec.getZ()); }

    inline glm::quat toGlm(btQuaternion const& quat)
    {
        return glm::quat(quat.getW(), quat.getX(), quat.getY(), quat.getZ());
    }

    inline bool is_nan(f32 v)
    {
        static constexpr u32 NanMask = 0x7fc00000;

        u32* v_u = reinterpret_cast<u32*>(&v);
        return (*v_u & NanMask) == NanMask;
    }

    inline glm::fmat4x3 toGlm(const btTransform& transform)
    {
        const btMatrix3x3 basis = transform.getBasis();

        const glm::fvec3 translation = toGlm(transform.getOrigin());

        return glm::fmat4x3(toGlm(basis[0]), toGlm(basis[1]), toGlm(basis[2]), translation);
    }

    inline btVector3 toBt(glm::vec3 const& vec) { return btVector3(vec.x, vec.y, vec.z); }

    inline btMatrix3x3 m33toBt(const glm::fmat3x3& m)
    {
        return btMatrix3x3(m[0].x, m[1].x, m[2].x, m[0].y, m[1].y, m[2].y, m[0].z, m[1].z, m[2].z);
    }

    inline btQuaternion toBt(glm::quat const& quat) { return btQuaternion(quat.w, quat.x, quat.y, quat.z); }

    inline btTransform toBt(const glm::fmat4x3& transform)
    {
        btMatrix3x3 mat = m33toBt(glm::fmat3x3(transform));
        btTransform ret = btTransform(mat, toBt(glm::column(transform, 3)));

        return ret;
    }

    static const int SimulationMaxSubStep = 3; // FIXME

    glm::fvec3 forward() { return glm::vec3(1.0f, 0.0f, 0.0f); }
    glm::fvec3 up() { return glm::vec3(0.0f, 1.0f, 0.0f); }

    /*
    void steer(float dtSecs, Movement& movement, float amount)
    {
        float steerMultiplier = 0.035f;
        float inclinationMax = 2.6f;
        float inclForce = (log(glm::length(movement.speed * 0.5f) + 1.0f) + 0.2f) * amount;
        float inclinationRepel = -movement.inclination * 5.0f;

        movement.inclination =
            glm::clamp(movement.inclination + (inclForce + inclinationRepel) * dtSecs, -inclinationMax, inclinationMax);
        movement.orientation *= glm::angleAxis(-amount * steerMultiplier, up());
    }
    */

    struct ShipInput
    {
        float brake;    // 0.0 not braking - 1.0 max
        float throttle; // 0.0 no trottle - 1.0 max
        float steer;    // 0.0 no steering - -1.0 max left - 1.0 max right
    };

    void pre_tick(PhysicsSim& sim, float dt)
    {
        REAPER_PROFILE_SCOPE_FUNC();

        (void)dt;
        // ShipInput ship_input = {};

        btRigidBody*       playerRigidBody = sim.players[0];
        const glm::fmat4x3 player_transform = toGlm(playerRigidBody->getWorldTransform());
        const glm::fvec3   player_position_ws = player_transform[3];
        const glm::quat    player_orientation = toGlm(playerRigidBody->getWorldTransform().getRotation());

        float              min_dist_sq = 10000000.f;
        const btRigidBody* closest_track_chunk = nullptr;
        for (auto track_chunk : sim.static_meshes)
        {
            glm::fvec3 delta = player_position_ws - toGlm(track_chunk->getWorldTransform().getOrigin());
            float      dist_sq = glm::dot(delta, delta);

            if (dist_sq < min_dist_sq)
            {
                min_dist_sq = dist_sq;
                closest_track_chunk = track_chunk;
            }
        }

        Assert(closest_track_chunk);

        // FIXME const glm::fmat4x3 projected_transform = toGlm(closest_track_chunk->getWorldTransform());
        const glm::quat projected_orientation = toGlm(closest_track_chunk->getWorldTransform().getRotation());
        glm::vec3       shipUp;
        glm::vec3       shipFwd;

        shipUp = projected_orientation * up();
        shipFwd = player_orientation * forward();
        // FIXME shipFwd = glm::normalize(shipFwd - glm::proj(shipFwd, shipUp)); // Ship Fwd in on slice plane

        // Re-eval ship orientation
        // FIXME player_orientation = glm::quat(glm::mat3(shipFwd, shipUp, glm::cross(shipFwd, shipUp)));

        // change steer angle
        // steer(dt, movement, factors.getTurnFactor());

        // Integrate forces
        glm::vec3 forces = {};
        forces += -shipUp * 98.331f; // 10x earth gravity
        // forces += shipFwd * factors.getThrustFactor() * player->getAcceleration(); // Engine thrust
        //      forces += shipFwd * player.getAcceleration(); // Engine thrust
        // forces += -glm::proj(movement.speed, shipFwd) * factors.getBrakeFactor() * sim.pBrakesForce; // Brakes force
        // forces += -glm::proj(movement.speed, glm::cross(shipFwd, shipUp)) * sim.pHandling;           // Handling term
        const glm::fvec3 linear_speed = toGlm(playerRigidBody->getLinearVelocity());
        forces += -linear_speed * sim.pLinearFriction;                           // Linear friction term
        forces += -linear_speed * glm::length(linear_speed) * sim.pQuadFriction; // Quadratic friction term

        // Progressive repelling force that pushes the ship downwards
        // FIXME
        // float upSpeed = glm::length(movement.speed - glm::proj(movement.speed, shipUp));
        // if (projection.relativePosition.y > 2.0f && upSpeed > 0.0f)
        //     forces += player_orientation * (glm::vec3(0.0f, 2.0f - upSpeed, 0.0f) * 10.0f);

        playerRigidBody->clearForces();
        playerRigidBody->applyCentralForce(toBt(forces));
    }

    /*
    {
        shipUp = projection.orientation * glm::up();
        shipFwd = movement.orientation * glm::forward();
        shipFwd = glm::normalize(shipFwd - glm::proj(shipFwd, shipUp)); // Ship Fwd in on slice plane

        // Re-eval ship orientation
        movement.orientation = glm::quat(glm::mat3(shipFwd, shipUp, glm::cross(shipFwd, shipUp)));

        // change steer angle
        steer(dt, movement, factors.getTurnFactor());

        // sum all forces
        // forces += - shipUp * 98.331f; // 10x earth gravity
        forces += -shipUp * 450.0f;                                                // lot of times earth gravity
        forces += shipFwd * factors.getThrustFactor() * player->getAcceleration(); // Engine thrust
        forces += -glm::proj(movement.speed, shipFwd) * factors.getBrakeFactor() * _pBrakesForce; // Brakes force
        forces += -glm::proj(movement.speed, glm::cross(shipFwd, shipUp)) * _pHandling;           // Handling term
        forces += -movement.speed * _pLinearFriction;                                             // Linear friction
    term forces += -movement.speed * glm::length(movement.speed) * _pQuadFriction;                 // Quadratic friction
    term

        // Progressive repelling force that pushes the ship downwards
        float upSpeed = glm::length(movement.speed - glm::proj(movement.speed, shipUp));
        if (projection.relativePosition.y > 2.0f && upSpeed > 0.0f)
            forces += movement.orientation * (glm::vec3(0.0f, 2.0f - upSpeed, 0.0f) * 10.0f);

        playerRigidBody->clearForces();
        playerRigidBody->applyCentralForce(toBt(forces));
    }
    */

    void post_tick(PhysicsSim& sim, float /*dt*/)
    {
        REAPER_PROFILE_SCOPE_FUNC();

        btRigidBody* playerRigidBody = sim.players.front(); // FIXME
        toGlm(playerRigidBody->getLinearVelocity());
        toGlm(playerRigidBody->getOrientation());
    }
#endif
} // namespace

PhysicsSim create_sim()
{
    PhysicsSim sim = {};

    // Physics tweakables
    sim.pLinearFriction = 4.0f;
    sim.pQuadFriction = 0.01f;
    sim.pHandling = 30.0f;
    sim.pBrakesForce = 30.0f;

#if defined(REAPER_USE_BULLET_PHYSICS)
    // Boilerplate code for a standard rigidbody simulation
    sim.broadphase = new btDbvtBroadphase();
    sim.collisionConfiguration = new btDefaultCollisionConfiguration();
    sim.dispatcher = new btCollisionDispatcher(sim.collisionConfiguration);
    btGImpactCollisionAlgorithm::registerAlgorithm(sim.dispatcher);
    sim.solver = new btSequentialImpulseConstraintSolver;

    // Putting all of that together
    sim.dynamicsWorld =
        new btDiscreteDynamicsWorld(sim.dispatcher, sim.broadphase, sim.solver, sim.collisionConfiguration);
#endif

    return sim;
}

void destroy_sim(PhysicsSim& sim)
{
#if defined(REAPER_USE_BULLET_PHYSICS)
    for (auto rb : sim.static_meshes)
    {
        sim.dynamicsWorld->removeRigidBody(rb);
        delete rb->getMotionState();
        delete rb->getCollisionShape();
        delete rb;
    }

    for (auto vb : sim.vertexArrayInterfaces)
        delete vb;

    for (auto player_rigid_body : sim.players)
    {
        sim.dynamicsWorld->removeRigidBody(player_rigid_body);
        delete player_rigid_body->getMotionState();
        delete player_rigid_body->getCollisionShape();
        delete player_rigid_body;
    }

    // Delete the rest of the bullet context
    delete sim.dynamicsWorld;
    delete sim.solver;
    delete sim.dispatcher;
    delete sim.collisionConfiguration;
    delete sim.broadphase;
#else
    static_cast<void>(sim);
#endif
}

namespace
{
#if defined(REAPER_USE_BULLET_PHYSICS)
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
#endif
} // namespace

void sim_start(PhysicsSim* sim)
{
    REAPER_PROFILE_SCOPE_FUNC();

    Assert(sim);

#if defined(REAPER_USE_BULLET_PHYSICS)
    sim->dynamicsWorld->setInternalTickCallback(pre_tick_callback, sim, true);
    sim->dynamicsWorld->setInternalTickCallback(post_tick_callback, sim, false);
#endif
}

void sim_update(PhysicsSim& sim, float dt)
{
    REAPER_PROFILE_SCOPE_FUNC();

#if defined(REAPER_USE_BULLET_PHYSICS)
    sim.dynamicsWorld->stepSimulation(dt, SimulationMaxSubStep);
#else
    static_cast<void>(sim);
    static_cast<void>(dt);
#endif
}

glm::fmat4x3 get_player_transform(PhysicsSim& sim)
{
#if defined(REAPER_USE_BULLET_PHYSICS)
    const btRigidBody*   playerRigidBody = sim.players.front();
    const btMotionState* playerMotionState = playerRigidBody->getMotionState();

    btTransform output;
    playerMotionState->getWorldTransform(output);

    return toGlm(output);
#else
    static_cast<void>(sim);
    AssertUnreachable();
    return glm::fmat4x3(1.f); // FIXME
#endif
}

void sim_register_static_collision_meshes(PhysicsSim& sim, const nonstd::span<Mesh> meshes,
                                          const nonstd::span<glm::fmat4x3> transforms)
{
#if defined(REAPER_USE_BULLET_PHYSICS)
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
#else
    static_cast<void>(sim);
    static_cast<void>(meshes);
    static_cast<void>(transforms);
#endif
}

void sim_create_player_rigid_body(PhysicsSim& sim, const glm::fmat4x3& player_transform, const glm::fvec3& shape_extent)
{
#if defined(REAPER_USE_BULLET_PHYSICS)
    btMotionState*    motionState = new btDefaultMotionState(toBt(player_transform));
    btCollisionShape* collisionShape = new btBoxShape(toBt(shape_extent));

    btScalar  mass = 10.f; // FIXME
    btVector3 inertia(0.f, 0.f, 0.f);

    collisionShape->calculateLocalInertia(mass, inertia);
    btRigidBody::btRigidBodyConstructionInfo constructInfo(mass, motionState, collisionShape, inertia);

    btRigidBody* player_rigid_body = new btRigidBody(constructInfo);
    player_rigid_body->setFriction(0.1f);
    player_rigid_body->setRollingFriction(0.8f);

    // FIXME doesn't do anything? Wrong collision mesh?
    player_rigid_body->setCcdMotionThreshold(0.0005f);
    player_rigid_body->setCcdSweptSphereRadius(shape_extent.x);

    sim.dynamicsWorld->addRigidBody(player_rigid_body);
    sim.players.push_back(player_rigid_body);
#else
    static_cast<void>(sim);
    static_cast<void>(player_transform);
    static_cast<void>(shape_extent);
#endif
}
} // namespace SplineSonic
