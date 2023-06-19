////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PhysicsSimUpdate.h"

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
namespace
{
    glm::fvec3 forward()
    {
        return glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::fvec3 up()
    {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }

    glm::fvec3 get_interpolated_up_vector_ws(const TrackSkeletonNode& skeleton_node)
    {}

#if defined(REAPER_USE_BULLET_PHYSICS)
    void pre_tick(const PhysicsSim& sim, const PhysicsSim::FrameData& frame_data, float /*dt*/)
    {
        REAPER_PROFILE_SCOPE_FUNC();

        btRigidBody*       playerRigidBody = sim.players[0];
        const glm::fmat4x3 player_transform = toGlm(playerRigidBody->getWorldTransform());
        const glm::fvec3   player_position_ws = player_transform[3];

        float                    min_dist_sq = 10000000.f;
        const TrackSkeletonNode* closest_skeleton_node = nullptr;

        Assert(frame_data.skeleton_nodes.size() == sim.static_mesh_colliders.size());

        for (const auto& skeleton_node : frame_data.skeleton_nodes)
        {
            const glm::fvec3 position_delta = player_position_ws - skeleton_node.center_ws;
            const float      dist_sq = glm::dot(position_delta, position_delta);

            if (dist_sq < min_dist_sq)
            {
                min_dist_sq = dist_sq;
                closest_skeleton_node = &skeleton_node;
            }
        }

        glm::fmat3x3 gravity_frame;

        Assert(closest_skeleton_node);

        if (closest_skeleton_node)
        {
            const TrackSkeletonNode& skeleton_node = *closest_skeleton_node;

            const glm::fvec3 player_position_in_ms =
                skeleton_node.in_transform_ws_to_ms * glm::fvec4(player_position_ws, 1.f);
            const glm::fvec3 player_position_out_ms =
                skeleton_node.out_transform_ws_to_ms * glm::fvec4(player_position_ws, 1.f);

            if (player_position_in_ms.x < 0.f)
            {
                gravity_frame = skeleton_node.in_transform_ms_to_ws;
            }
            else if (player_position_out_ms.x > 0.f)
            {
                gravity_frame = skeleton_node.out_transform_ms_to_ws;
            }
            else
            {
                // In between, or with planes crossed
                // FIXME
                gravity_frame = skeleton_node.in_transform_ms_to_ws;
            }
        }

        const glm::fvec3 gravity_up_ws = gravity_frame * up();
        const glm::fvec3 player_forward_ws = player_transform * glm::vec4(forward(), 0.f);
        const glm::fvec3 player_up_ws = player_transform * glm::vec4(up(), 0.f);
        const glm::fvec3 player_linear_speed = toGlm(playerRigidBody->getLinearVelocity());
        const glm::fvec3 player_angular_speed = toGlm(playerRigidBody->getAngularVelocity());

        glm::fvec3 force_ws = {};

        // Gravity
        force_ws += gravity_up_ws * -sim.vars.gravity_force_intensity;

        // Throttle
        force_ws += player_forward_ws * frame_data.input.throttle * sim.vars.default_ship_stats.thrust;

        // Brake
        force_ws += -glm::proj(player_linear_speed, player_forward_ws) * frame_data.input.brake
                    * sim.vars.default_ship_stats.braking;

        // Friction force (FIXME might be better applied by bullet directly)
        force_ws += -player_linear_speed * sim.vars.linear_friction;
        force_ws += -player_linear_speed * glm::length(player_linear_speed) * sim.vars.quadratic_friction;

        glm::fvec3 torque_ws = {};

        // Turning torque
        torque_ws += player_up_ws * frame_data.input.steer;
        // torque_ws += glm::eulerAngles(glm::angleAxis(frame_data.input.steer, player_up_ws));

        // Torque to orient the ship upright
        {
            const float dot_gravity_to_player = glm::dot(player_up_ws, gravity_up_ws);

            const glm::fvec3 u_cross_v = glm::cross(player_up_ws, gravity_up_ws);
            const float      u_dot_v_plus_one = 1.f + dot_gravity_to_player;

            const glm::quat  delta = glm::normalize(glm::quat(glm::fvec4(u_cross_v, u_dot_v_plus_one)));
            const glm::fvec3 torque = glm::eulerAngles(delta) * -1.85f;

            torque_ws += torque;
        }

        // Friction torque (FIXME might be better applied by bullet directly)
        torque_ws += -player_angular_speed * sim.vars.angular_friction;

        playerRigidBody->clearForces();
        playerRigidBody->applyTorque(toBt(torque_ws));
        playerRigidBody->applyCentralForce(toBt(force_ws));
    }

    // FIXME Handling force to pull you inside when cornering
    // const glm::fvec3 player_side_ws = glm::cross(player_forward_ws, player_up_ws);
    // force_ws += -glm::proj(player_linear_speed, player_side_ws) * sim.vars.default_ship_stats.handling;

    // change steer angle
    // FIXME this should probably be cosmetic only
    // float steerMultiplier = 0.035f;
    // float inclinationMax = 2.6f;
    // float inclForce = (log(glm::length(player_linear_speed * 0.5f) + 1.0f) + 0.2f) * input.steer;
    //  float inclinationRepel = -movement.inclination * 5.0f;

    // movement.inclination =
    //     glm::clamp(movement.inclination + (inclForce + inclinationRepel) * dtSecs, -inclinationMax,
    //     inclinationMax);
    //  movement.orientation *= glm::angleAxis(-input.steer * steerMultiplier, up());

    // Progressive repelling force that pushes the ship downwards
    // FIXME
    // float upSpeed = glm::length(movement.speed - glm::proj(movement.speed, shipUp));
    // if (projection.relativePosition.y > 2.0f && upSpeed > 0.0f)
    //     force_ws += player_orientation * (glm::vec3(0.0f, 2.0f - upSpeed, 0.0f) * 10.0f);

    void post_tick(const PhysicsSim& sim, float /*dt*/)
    {
        REAPER_PROFILE_SCOPE_FUNC();

        btRigidBody* playerRigidBody = sim.players.front(); // FIXME
        toGlm(playerRigidBody->getLinearVelocity());
        toGlm(playerRigidBody->getOrientation());
    }
#endif

#if defined(REAPER_USE_BULLET_PHYSICS)
    void pre_tick_callback(btDynamicsWorld* world, btScalar dt)
    {
        PhysicsSim* sim_ptr = static_cast<PhysicsSim*>(world->getWorldUserInfo());
        Assert(sim_ptr);

        PhysicsSim& sim = *sim_ptr;

        pre_tick(sim, sim.frame_data, dt);
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

void sim_update(PhysicsSim& sim, nonstd::span<const TrackSkeletonNode> skeleton_nodes, const ShipInput& input, float dt)
{
    REAPER_PROFILE_SCOPE_FUNC();

    // FIXME we should be able to provide input for each discrete sim step for greater accuration.
    // Maybe that doesn't matter now, but at polish time this might be a big deal
    sim.frame_data.input = input; // FIXME
    sim.frame_data.skeleton_nodes = skeleton_nodes;

#if defined(REAPER_USE_BULLET_PHYSICS)
    sim.dynamicsWorld->stepSimulation(dt, sim.vars.max_simulation_substep_count, sim.vars.simulation_substep_duration);
#else
    static_cast<void>(sim);
    static_cast<void>(dt);
#endif
}
} // namespace Neptune
