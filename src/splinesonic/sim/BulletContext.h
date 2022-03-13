#ifndef BULLETCONTEXT_H
#define BULLETCONTEXT_H

#include <vector>

#include <btBulletDynamicsCommon.h>

#include "Player/IPlayer.h"
#include "Track.h"

class BulletContext
{
    struct PlayerBtInfo
    {
        btRigidBody* body;
        IPlayer*     player;
    };

    /**
     * @anchor SimulationMaxSubStep controls the allowed maximum for bullet's
     * internal simulation substeps.
     * It also means that if a frame time reaches SimulationMaxSubStep times
     * the base step time (usually the screen refresh rate), the physics
     * simulation will be behind the real time clock at the end of the
     * simulation step. This case is only problematic when the physics are
     * networked, and should be handled by the prediction code.
     */
    static const int SimulationMaxSubStep = 8;

public:
    BulletContext();
    ~BulletContext();

public:
    /**
     * This functions is called from bullet before every internal tick,
     * to let us update forces applied on rigid bodies.
     *
     * NOTE: DO NOT CALL MANUALLY
     * @param dt internal tick duration
     */
    void internalBulletPreTick(float dt);

    /**
     * This functions is called from bullet after every internal tick.
     * Checking for track boundaries can be done here.
     *
     * NOTE: DO NOT CALL MANUALLY
     * @param dt internal tick duration
     */
    void internalBulletPostTick(float dt);

    /**
     * Let bullet step the simulation. it will update MotionStates accordingly,
     * but several internal steps can be executed, controlled by
     * @ref SimulationMaxSubStep.
     * Only one call should be made for each frame for the MotionStates to work
     * correctly.
     * FIXME This should be called with a reasonably precise time, currently
     * it only uses millisecond precision, inducing a 4% relative error
     * on a standard frame.
     * @param dt tick duration
     */
    void update(float dt, IPlayer& player);

    /**
     * Feed the track geometry to bullet so it can create a collision shape.
     * @param track this game's track instance
     */
    void buildTrackCollisionShape(const Track& track);

private:
    /**
     * Create the rigid body corresponding to the player
     * @param player instance of the player
     */
    void createPlayerRigidBody(IPlayer& player);

private:
    float _pLinearFriction;
    float _pQuadFriction;
    float _pHandling;
    float _pBrakesForce;

private:
    btBroadphaseInterface*               _broadphase;
    btDefaultCollisionConfiguration*     _collisionConfiguration;
    btCollisionDispatcher*               _dispatcher;
    btSequentialImpulseConstraintSolver* _solver;
    btDiscreteDynamicsWorld*             _dynamicsWorld;

private:
    float                                 _simulationTime;
    std::vector<btRigidBody*>             _rigidBodies;
    std::vector<PlayerBtInfo>             _players;
    std::vector<btStridingMeshInterface*> _vertexArrayInterfaces;
    Track const*                          _track;
};

#endif // BULLETCONTEXT_H
