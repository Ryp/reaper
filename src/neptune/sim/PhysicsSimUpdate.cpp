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
#include <glm/gtx/norm.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/vec3.hpp>

namespace Neptune
{
namespace
{
#if defined(REAPER_USE_BULLET_PHYSICS)
    void reset_player_position(
        btRigidBody* rigid_body, std::span<RaycastSuspension> raycast_suspensions, const glm::fmat4x3& transform)
    {
        // Reset all velocities
        rigid_body->setWorldTransform(toBt(transform));
        rigid_body->setLinearVelocity(toBt(glm::fvec3()));
        rigid_body->setAngularVelocity(toBt(glm::fvec3()));
        rigid_body->setTurnVelocity(toBt(glm::fvec3()));
        rigid_body->setPushVelocity(toBt(glm::fvec3()));

        // Reset suspension state
        // NOTE: maybe it's better to use the minimum travel distance in this case?
        for (auto& suspension : raycast_suspensions)
        {
            suspension.length_ratio_last = suspension.length_ratio_rest;
        }
    }

    void update_forces(PhysicsSim& sim, const PhysicsSim::FrameData& frame_data, float dt)
    {
        // FIXME: getMotionState() could be used to get interpolated positions (might be better)
        REAPER_PROFILE_SCOPE_FUNC();

        btRigidBody*       player_rigid_body = sim.players.front(); // FIXME
        const glm::fmat4x3 player_transform = toGlm(player_rigid_body->getWorldTransform());
        const glm::fvec3   player_position_ws = player_transform[3];

        // NOTE: We need to do this everyframe anyway, otherwise it's kept from last sim frame
        player_rigid_body->clearForces();

        float                    min_center_dist_sq = 10000000.f;
        const TrackSkeletonNode* closest_skeleton_node = nullptr;

        Assert(frame_data.skeleton_nodes.size() == sim.static_mesh_colliders.size());

        for (const auto& skeleton_node : frame_data.skeleton_nodes)
        {
            const float player_to_center_distance_sq = glm::distance2(player_position_ws, skeleton_node.center_ws);

            if (player_to_center_distance_sq < min_center_dist_sq)
            {
                min_center_dist_sq = player_to_center_distance_sq;
                closest_skeleton_node = &skeleton_node;
            }
        }

        glm::fmat3x3 gravity_frame;

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
        else
        {
            AssertUnreachable();
            gravity_frame = glm::identity<glm::fmat3x3>();
        }

        sim.last_gravity_frame = gravity_frame;

        if (closest_skeleton_node)
        {
            const float respawn_distance = closest_skeleton_node->radius * 2.f;
            const float respawn_distance_sq = respawn_distance * respawn_distance;
            if (min_center_dist_sq > respawn_distance_sq)
            {
                glm::fmat4x3 new_player_transform = glm::mat4(closest_skeleton_node->in_transform_ms_to_ws)
                                                    * glm::translate(glm::identity<glm::mat4>(), up() * 1.f);

                reset_player_position(player_rigid_body, sim.raycast_suspensions, new_player_transform);
                return;
            }
        }

        const glm::fvec3 gravity_up_ws = gravity_frame * up();
        const glm::fvec3 player_forward_ws = player_transform * glm::vec4(forward(), 0.f);
        const glm::fvec3 player_up_ws = player_transform * glm::vec4(up(), 0.f);
        const glm::fvec3 player_linear_speed = toGlm(player_rigid_body->getLinearVelocity());
        // const glm::fvec3 player_angular_speed = toGlm(player_rigid_body->getAngularVelocity());

        glm::fvec3 force_ws = {};

        // Gravity
        force_ws += gravity_up_ws * -sim.vars.gravity_force_intensity;

        // Throttle
        force_ws += player_forward_ws * frame_data.input.throttle * sim.vars.default_ship_stats.thrust;

        // Brake
        force_ws += -glm::proj(player_linear_speed, player_forward_ws) * frame_data.input.brake
                    * sim.vars.default_ship_stats.braking;

        glm::fvec3 torque_ws = {};

        // Turning torque
        torque_ws += player_up_ws * -frame_data.input.steer * sim.vars.steer_force;

        // Torque to orient the ship upright
        if (false) // FIXME
        {
            const float dot_gravity_to_player = glm::dot(player_up_ws, gravity_up_ws);

            const glm::fvec3 u_cross_v = glm::cross(player_up_ws, gravity_up_ws);
            const float      u_dot_v_plus_one = 1.f + dot_gravity_to_player;

            const glm::quat  delta = glm::normalize(glm::quat(glm::fvec4(u_cross_v, u_dot_v_plus_one)));
            const glm::fvec3 torque = glm::eulerAngles(delta) * 0.85f;

            torque_ws += torque;
        }

        player_rigid_body->setDamping(sim.vars.linear_friction, sim.vars.angular_friction);

        player_rigid_body->applyTorque(toBt(torque_ws));
        player_rigid_body->applyCentralForce(toBt(force_ws));

        for (auto& suspension : sim.raycast_suspensions)
        {
            const glm::fmat4x3 chassis_transform = player_transform;

            const glm::fvec3 suspension_position_start_ws =
                chassis_transform * glm::fvec4(suspension.position_start_ms, 1.f);
            const glm::fvec3 suspension_position_end_ws =
                chassis_transform * glm::fvec4(suspension.position_end_ms, 1.f);
            const btVector3 suspension_direction_ws =
                (toBt(suspension_position_end_ws - suspension_position_start_ws)).normalized();

            btCollisionWorld::ClosestRayResultCallback ray_result_callback(toBt(suspension_position_start_ws),
                                                                           toBt(suspension_position_end_ws));

            suspension.position_start_ws = suspension_position_start_ws;
            suspension.position_end_ws = suspension_position_end_ws;

            sim.dynamics_world->rayTest(
                toBt(suspension_position_start_ws), toBt(suspension_position_end_ws), ray_result_callback);

            const float chassisMass = 1.f / player_rigid_body->getInvMass(); // FIXME or get the constant directly

            static constexpr bool force_defaults = true;

            if (ray_result_callback.hasHit())
            {
                const btRigidBody* body = btRigidBody::upcast(ray_result_callback.m_collisionObject);

                if (body && body->hasContactResponse())
                {
                    const btVector3 ray_hit_position_ws = ray_result_callback.m_hitPointWorld;
                    const btVector3 ray_hit_normal_ws =
                        ray_result_callback.m_hitNormalWorld.normalized(); // FIXME is that necessary?

                    //
                    float denominator = ray_hit_normal_ws.dot(suspension_direction_ws);

                    btVector3 ray_hit_position_ms = ray_hit_position_ws - player_rigid_body->getCenterOfMassPosition();
                    btVector3 body_velocity_at_ray_hit_ws =
                        player_rigid_body->getVelocityInLocalPoint(ray_hit_position_ms);

                    float suspension_velocity = ray_hit_normal_ws.dot(body_velocity_at_ray_hit_ws);

                    float suspension_relative_velocity;
                    float suspension_clippedInvContactDotSuspension;

                    if (denominator >= -0.1f)
                    {
                        suspension_relative_velocity = 0.f;
                        suspension_clippedInvContactDotSuspension = 1.f / 0.1f;
                    }
                    else
                    {
                        const float inv = -1.f / denominator;
                        suspension_relative_velocity = suspension_velocity * inv;
                        suspension_clippedInvContactDotSuspension = inv;
                    }
                    //

                    float suspension_force = 0.f;

                    // Spring
                    {
                        const float stiffness =
                            force_defaults ? sim.vars.default_spring_stiffness : suspension.spring_stiffness;
                        const float length_ratio_diff =
                            (ray_result_callback.m_closestHitFraction - suspension.length_ratio_rest);

                        suspension_force -= stiffness * length_ratio_diff * suspension.length_max
                                            * suspension_clippedInvContactDotSuspension;
                    }

                    // Damper
                    {
                        float susp_damping;
                        if (suspension_relative_velocity < 0.f)
                        {
                            susp_damping = force_defaults ? sim.vars.default_damper_friction_compression
                                                          : suspension.damper_friction_compression;
                        }
                        else
                        {
                            susp_damping = force_defaults ? sim.vars.default_damper_friction_extension
                                                          : suspension.damper_friction_extension;
                        }
                        suspension_force -= susp_damping * suspension_relative_velocity;
                    }

                    // RESULT
                    suspension_force *= chassisMass; // FIXME this is making the force weight independent?
                    suspension_force = glm::clamp(suspension_force, 0.f, sim.vars.max_suspension_force);

                    btVector3 impulse = ray_hit_normal_ws * suspension_force * dt;
                    btVector3 force_relpos = ray_hit_position_ws - player_rigid_body->getCenterOfMassPosition();

                    if (sim.vars.enable_suspension_forces)
                    {
                        player_rigid_body->applyImpulse(impulse, force_relpos);
                    }

                    // Update dynamic suspension data FIXME this makes the sim non-const
                    suspension.length_ratio_last = ray_result_callback.m_closestHitFraction;
                }
            }
            else
            {
                suspension.length_ratio_last = suspension.length_ratio_rest;
            }
        }
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
#endif
} // namespace
} // namespace Neptune

#if defined(REAPER_USE_BULLET_PHYSICS)
static void pre_tick_callback(btDynamicsWorld* world, btScalar dt)
{
    using namespace Neptune;

    PhysicsSim* sim_ptr = static_cast<PhysicsSim*>(world->getWorldUserInfo());
    Assert(sim_ptr);

    PhysicsSim& sim = *sim_ptr;

    update_forces(sim, sim.frame_data, dt);
}
#endif

namespace Neptune
{
void sim_start(PhysicsSim* sim)
{
    REAPER_PROFILE_SCOPE_FUNC();

    Assert(sim);

#if defined(REAPER_USE_BULLET_PHYSICS)
    sim->dynamics_world->setInternalTickCallback(pre_tick_callback, sim, true);
#endif
}

void sim_update(PhysicsSim& sim, std::span<const TrackSkeletonNode> skeleton_nodes, const ShipInput& input, float dt)
{
    REAPER_PROFILE_SCOPE_FUNC();

    // FIXME we should be able to provide input for each discrete sim step for greater accuration.
    // Maybe that doesn't matter now, but at polish time this might be a big deal
    sim.frame_data.input = input; // FIXME
    sim.frame_data.skeleton_nodes = skeleton_nodes;

#if defined(REAPER_USE_BULLET_PHYSICS)
    sim.dynamics_world->stepSimulation(dt, sim.vars.max_simulation_substep_count, sim.vars.simulation_substep_duration);
#else
    static_cast<void>(sim);
    static_cast<void>(dt);
#endif
}
} // namespace Neptune
