////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PhysicsSim.h"

#if defined(REAPER_USE_BULLET_PHYSICS)
#    include "BulletConversion.inl"
#    include <BulletCollision/CollisionShapes/btBoxShape.h>
#    include <BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.h>
#    include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#    include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#    include <btBulletDynamicsCommon.h>
#endif

#include "neptune/trackgen/Track.h"

#include "mesh/Mesh.h"

#include "core/Assert.h"
#include "profiling/Scope.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/vec3.hpp>

namespace Neptune
{
static constexpr float ShipMass = 1.f;

PhysicsSim create_sim()
{
    PhysicsSim sim = {};

    // Physics runtime tweakables
    sim.vars.simulation_substep_duration = 1.f / 60.f;
    sim.vars.max_simulation_substep_count = 3;
    sim.vars.gravity_force_intensity = 9.8331f;
    sim.vars.linear_friction = 4.0f;
    sim.vars.quadratic_friction = 0.01f;
    sim.vars.angular_friction = 0.3f;
    sim.vars.default_ship_stats.thrust = 100.f;
    sim.vars.default_ship_stats.braking = 10.f;
    sim.vars.default_ship_stats.handling = 0.4f;

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

    btScalar mass = ShipMass;

    btVector3 inertia;
    collisionShape->calculateLocalInertia(mass, inertia);

    btRigidBody::btRigidBodyConstructionInfo constructInfo(mass, motionState, collisionShape, inertia);

    btRigidBody* player_rigid_body = new btRigidBody(constructInfo);
    // player_rigid_body->setFriction(0.9f);
    // player_rigid_body->setRollingFriction(9.8f);

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
