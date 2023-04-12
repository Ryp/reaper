////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Test.h"

#if defined(REAPER_USE_BULLET_PHYSICS)
#    include <BulletCollision/CollisionShapes/btBoxShape.h>
#    include <BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.h>
#    include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#    include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#    include <btBulletDynamicsCommon.h>
#endif

#include "mesh/Mesh.h"

#include "core/Assert.h"
#include "profiling/Scope.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/vec3.hpp>

namespace Neptune
{
namespace
{
#if defined(REAPER_USE_BULLET_PHYSICS)
    inline glm::vec3 toGlm(btVector3 const& vec)
    {
        return glm::vec3(vec.getX(), vec.getY(), vec.getZ());
    }

    inline glm::quat toGlm(btQuaternion const& quat)
    {
        return glm::quat(quat.getW(), quat.getX(), quat.getY(), quat.getZ());
    }

    inline glm::fmat3x3 toGlm(const btMatrix3x3& basis)
    {
        return glm::fmat3x3(toGlm(basis[0]), toGlm(basis[1]), toGlm(basis[2]));
    }

    inline glm::fmat4x3 toGlm(const btTransform& transform)
    {
        const btMatrix3x3 basis = transform.getBasis();

        const glm::fvec3 translation = toGlm(transform.getOrigin());

        return glm::fmat4x3(toGlm(basis[0]), toGlm(basis[1]), toGlm(basis[2]), translation);
    }

    inline btVector3 toBt(glm::vec3 const& vec)
    {
        return btVector3(vec.x, vec.y, vec.z);
    }

    inline btMatrix3x3 m33toBt(const glm::fmat3x3& m)
    {
        return btMatrix3x3(m[0].x, m[1].x, m[2].x, m[0].y, m[1].y, m[2].y, m[0].z, m[1].z, m[2].z);
    }

    // inline btQuaternion toBt(glm::quat const& quat) { return btQuaternion(quat.w, quat.x, quat.y, quat.z); }

    inline btTransform toBt(const glm::fmat4x3& transform)
    {
        btMatrix3x3 mat = m33toBt(glm::fmat3x3(transform));
        btTransform ret = btTransform(mat, toBt(glm::column(transform, 3)));

        return ret;
    }

    static const int SimulationMaxSubStep = 3; // FIXME

    glm::fvec3 forward()
    {
        return glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::fvec3 up()
    {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }

    void pre_tick(PhysicsSim& sim, const ShipInput& input, float /*dt*/)
    {
        REAPER_PROFILE_SCOPE_FUNC();

        btRigidBody*       playerRigidBody = sim.players[0];
        const glm::fmat4x3 player_transform = toGlm(playerRigidBody->getWorldTransform());
        const glm::fvec3   player_position_ws = player_transform[3];

        float              min_dist_sq = 10000000.f;
        const btRigidBody* closest_track_chunk = nullptr;

        // FIXME correctly iterate on track chunks only
        for (auto mesh_collider : sim.static_mesh_colliders)
        {
            auto       track_chunk = mesh_collider.second.rigid_body;
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

        // shipUp = projected_orientation * up();
        shipUp = up(); // FIXME
        shipFwd = player_transform * glm::vec4(forward(), 0.f);

        // shipFwd = glm::normalize(shipFwd - glm::proj(shipFwd, shipUp)); // Ship Fwd in on slice plane

        // Re-eval ship orientation
        // FIXME player_orientation = glm::quat(glm::mat3(shipFwd, shipUp, glm::cross(shipFwd, shipUp)));

        const glm::fvec3 linear_speed = toGlm(playerRigidBody->getLinearVelocity());

        // change steer angle
        {
            // float steerMultiplier = 0.035f;
            // float inclinationMax = 2.6f;
            // float inclForce = (log(glm::length(linear_speed * 0.5f) + 1.0f) + 0.2f) * input.steer;
            //  float inclinationRepel = -movement.inclination * 5.0f;

            // movement.inclination =
            //     glm::clamp(movement.inclination + (inclForce + inclinationRepel) * dtSecs, -inclinationMax,
            //     inclinationMax);
            //  movement.orientation *= glm::angleAxis(-input.steer * steerMultiplier, up());
        }

        ShipStats ship_stats;
        ship_stats.thrust = 100.f;
        ship_stats.braking = 10.f;

        // Integrate forces
        glm::vec3 forces = {};
        forces += -shipUp * 9.8331f;
        forces += shipFwd * input.throttle * ship_stats.thrust; // Engine thrust
        //      forces += shipFwd * player.getAcceleration(); // Engine thrust
        forces += -glm::proj(linear_speed, shipFwd) * input.brake * ship_stats.braking; // Brakes force
        // forces += -glm::proj(movement.speed, glm::cross(shipFwd, shipUp)) * sim.pHandling;           // Handling term
        forces += -linear_speed * sim.linear_friction;
        forces += -linear_speed * glm::length(linear_speed) * sim.quadratic_friction;

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
        forces += -movement.speed * linear_friction;                                             // Linear friction
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
    sim.linear_friction = 4.0f;
    sim.quadratic_friction = 0.01f;

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
    Assert(sim.static_mesh_colliders.empty());

#if defined(REAPER_USE_BULLET_PHYSICS)
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
        PhysicsSim* sim_ptr = static_cast<PhysicsSim*>(world->getWorldUserInfo());
        Assert(sim_ptr);

        PhysicsSim& sim = *sim_ptr;

        pre_tick(sim, sim.last_input, dt);
    }

    void post_tick_callback(btDynamicsWorld* world, btScalar dt)
    {
        PhysicsSim* sim_ptr = static_cast<PhysicsSim*>(world->getWorldUserInfo());
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

void sim_update(PhysicsSim& sim, const ShipInput& input, float dt)
{
    REAPER_PROFILE_SCOPE_FUNC();

    sim.last_input = input; // FIXME

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

void sim_create_static_collision_meshes(nonstd::span<StaticMeshColliderHandle> handles, PhysicsSim& sim,
                                        nonstd::span<const Mesh>         meshes,
                                        nonstd::span<const glm::fmat4x3> transforms_no_scale,
                                        nonstd::span<const glm::fvec3>   scales)
{
    // FIXME Assert scale values
#if defined(REAPER_USE_BULLET_PHYSICS)
    Assert(handles.size() == meshes.size());
    Assert(meshes.size() == transforms_no_scale.size());
    Assert(scales.empty() || scales.size() == transforms_no_scale.size());

    for (u32 i = 0; i < meshes.size(); i++)
    {
        const Mesh& mesh = meshes[i];

        // Insert a new struct and get a reference on it
        // It's crucial to avoid copies here because we give pointers to the mesh data to bullet
        sim.static_mesh_colliders.emplace(sim.alloc_number_fixme, StaticMeshCollider{});
        StaticMeshCollider& mesh_collider = sim.static_mesh_colliders.at(sim.alloc_number_fixme);

        handles[i] = sim.alloc_number_fixme;
        sim.alloc_number_fixme += 1;

        // Deep copy vertex data
        mesh_collider.indices = mesh.indexes;
        mesh_collider.vertex_positions = mesh.positions;

        // Describe how the mesh is laid out in memory
        btIndexedMesh indexedMesh;
        indexedMesh.m_numTriangles = mesh_collider.indices.size() / 3;
        indexedMesh.m_triangleIndexBase = reinterpret_cast<const u8*>(mesh_collider.indices.data());
        indexedMesh.m_triangleIndexStride = 3 * sizeof(mesh_collider.indices[0]);
        indexedMesh.m_numVertices = mesh_collider.vertex_positions.size();
        indexedMesh.m_vertexBase = reinterpret_cast<const u8*>(mesh_collider.vertex_positions.data());
        indexedMesh.m_vertexStride = sizeof(mesh_collider.vertex_positions[0]);
        indexedMesh.m_indexType = PHY_INTEGER;
        indexedMesh.m_vertexType = PHY_FLOAT;

        // Create bullet collision shape based on the mesh described
        btTriangleIndexVertexArray* meshInterface = new btTriangleIndexVertexArray();

        meshInterface->addIndexedMesh(indexedMesh);
        btBvhTriangleMeshShape* meshShape = new btBvhTriangleMeshShape(meshInterface, true);

        const btVector3               scale = scales.empty() ? btVector3(1.f, 1.f, 1.f) : toBt(scales[i]);
        btScaledBvhTriangleMeshShape* scaled_mesh_shape = new btScaledBvhTriangleMeshShape(meshShape, scale);

        btDefaultMotionState* chunkMotionState = new btDefaultMotionState(btTransform(toBt(transforms_no_scale[i])));

        // Create the rigidbody with the collision shape
        btRigidBody::btRigidBodyConstructionInfo staticMeshRigidBodyInfo(0, chunkMotionState, scaled_mesh_shape);
        btRigidBody*                             staticRigidMesh = new btRigidBody(staticMeshRigidBodyInfo);

        mesh_collider.rigid_body = staticRigidMesh;
        mesh_collider.mesh_interface = meshInterface;
        mesh_collider.scaled_mesh_shape = scaled_mesh_shape;

        sim.dynamicsWorld->addRigidBody(mesh_collider.rigid_body);
    }
#else
    static_cast<void>(sim);
    static_cast<void>(meshes);
    static_cast<void>(transforms_no_scale);
    static_cast<void>(scales);
#endif
}

void sim_destroy_static_collision_meshes(nonstd::span<const StaticMeshColliderHandle> handles, PhysicsSim& sim)
{
    for (auto handle : handles)
    {
        const StaticMeshCollider& collider = sim.static_mesh_colliders.at(handle);

        sim.dynamicsWorld->removeRigidBody(collider.rigid_body);

        delete collider.rigid_body->getMotionState();
        delete collider.rigid_body;
        delete collider.scaled_mesh_shape->getChildShape();
        delete collider.scaled_mesh_shape;
        delete collider.mesh_interface;

        sim.static_mesh_colliders.erase(handle);
    }
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
    player_rigid_body->setFriction(0.9f);
    player_rigid_body->setRollingFriction(9.8f);

    player_rigid_body->setActivationState(DISABLE_DEACTIVATION);

    // FIXME doesn't do anything? Wrong collision mesh?
    player_rigid_body->setCcdMotionThreshold(0.05f);
    player_rigid_body->setCcdSweptSphereRadius(shape_extent.x);

    sim.dynamicsWorld->addRigidBody(player_rigid_body);
    sim.players.push_back(player_rigid_body);
#else
    static_cast<void>(sim);
    static_cast<void>(player_transform);
    static_cast<void>(shape_extent);
#endif
}
} // namespace Neptune
