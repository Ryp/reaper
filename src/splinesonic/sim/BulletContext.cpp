#include "BulletContext.h"

#include <functional>

#include "Physics/BulletUtils.h"

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

#include <glm/gtx/projection.hpp>

BulletContext::BulletContext()
    : _simulationTime(0.0f)
{
    // Physics tweakables
    _pLinearFriction = 4.0f;
    _pQuadFriction = 0.01f;
    _pHandling = 30.0f;
    _pBrakesForce = 30.0f;

    // Boilerplate code for a standard rigidbody simulation
    _broadphase = new btDbvtBroadphase();
    _collisionConfiguration = new btDefaultCollisionConfiguration();
    _dispatcher = new btCollisionDispatcher(_collisionConfiguration);
    btGImpactCollisionAlgorithm::registerAlgorithm(_dispatcher);
    _solver = new btSequentialImpulseConstraintSolver;

    // Putting all of that together
    _dynamicsWorld = new btDiscreteDynamicsWorld(_dispatcher, _broadphase, _solver, _collisionConfiguration);

    // FIXME is there a lifetime issue with this ?
    auto preTickCallback = [](btDynamicsWorld* world, btScalar dt) {
        auto context = static_cast<BulletContext*>(world->getWorldUserInfo());
        context->internalBulletPreTick(dt);
    };

    // FIXME is there a lifetime issue with this ?
    auto postTickCallback = [](btDynamicsWorld* world, btScalar dt) {
        auto context = static_cast<BulletContext*>(world->getWorldUserInfo());
        context->internalBulletPostTick(dt);
    };

    _dynamicsWorld->setInternalTickCallback(preTickCallback, static_cast<void*>(this), true);
    _dynamicsWorld->setInternalTickCallback(postTickCallback, static_cast<void*>(this), false);
}

BulletContext::~BulletContext()
{
    for (auto rb : _rigidBodies)
    {
        _dynamicsWorld->removeRigidBody(rb);
        delete rb->getMotionState();
        delete rb->getCollisionShape();
        delete rb;
    }
    for (auto vb : _vertexArrayInterfaces)
        delete vb;

    for (auto playerInfo : _players)
    {
        _dynamicsWorld->removeRigidBody(playerInfo.body);
        delete playerInfo.body->getMotionState();
        delete playerInfo.body->getCollisionShape();
        delete playerInfo.body;
    }
    // Delete the rest of the bullet context
    delete _dynamicsWorld;
    delete _solver;
    delete _dispatcher;
    delete _collisionConfiguration;
    delete _broadphase;
}

static void steer(float dtSecs, Movement& movement, float amount)
{
    float steerMultiplier = 0.035f;
    float inclinationMax = 2.6f;
    float inclForce = (log(glm::length(movement.speed * 0.5f) + 1.0f) + 0.2f) * amount;
    float inclinationRepel = -movement.inclination * 5.0f;

    movement.inclination =
        glm::clamp(movement.inclination + (inclForce + inclinationRepel) * dtSecs, -inclinationMax, inclinationMax);
    movement.orientation *= glm::angleAxis(-amount * steerMultiplier, glm::up());
}

void BulletContext::internalBulletPreTick(float dt)
{
    btRigidBody* playerRigidBody = _players[0].body;
    IPlayer*     player = _players[0].player;
    glm::vec3    forces;
    const auto&  factors = player->getMovementFactor();
    auto&        movement = player->getMovement();
    TrackChunk*  nearestChunk = _track->getNearestChunk(movement.position);
    auto         chunkNfo = nearestChunk->getInfo();
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
    forces += -shipUp * 450.0f;                                                // lot of times earth gravity
    forces += shipFwd * factors.getThrustFactor() * player->getAcceleration(); // Engine thrust
    //     forces += shipFwd * player.getAcceleration(); // Engine thrust
    forces += -glm::proj(movement.speed, shipFwd) * factors.getBrakeFactor() * _pBrakesForce; // Brakes force
    forces += -glm::proj(movement.speed, glm::cross(shipFwd, shipUp)) * _pHandling;           // Handling term
    forces += -movement.speed * _pLinearFriction;                                             // Linear friction term
    forces += -movement.speed * glm::length(movement.speed) * _pQuadFriction;                 // Quadratic friction term

    // Progressive repelling force that pushes the ship downwards
    float upSpeed = glm::length(movement.speed - glm::proj(movement.speed, shipUp));
    if (projection.relativePosition.y > 2.0f && upSpeed > 0.0f)
        forces += movement.orientation * (glm::vec3(0.0f, 2.0f - upSpeed, 0.0f) * 10.0f);

    playerRigidBody->clearForces();
    playerRigidBody->applyCentralForce(toBt(forces));
}

void BulletContext::internalBulletPostTick(float dt)
{
    btRigidBody* playerRigidBody = _players[0].body;
    IPlayer*     player = _players[0].player;

    player->getMovement().speed = toGlm(playerRigidBody->getLinearVelocity());
    player->getMovement().orientation = toGlm(playerRigidBody->getOrientation());

    _simulationTime += dt;
}

void BulletContext::update(float dt, IPlayer& player)
{
    if (_players.empty())
        createPlayerRigidBody(player);

    _dynamicsWorld->stepSimulation(dt, SimulationMaxSubStep);
}

void BulletContext::buildTrackCollisionShape(const Track& track)
{
    _track = &track;
    for (auto& chunk : track.getChunks())
    {
        auto& geometry = chunk->getGeometry();

        // Describe how the mesh is laid out in memory
        btIndexedMesh indexedMesh;
        indexedMesh.m_vertexType = PHY_FLOAT;
        indexedMesh.m_triangleIndexStride = sizeof(geometry.indexes[0]);
        indexedMesh.m_vertexStride = sizeof(geometry.vertices[0]);
        indexedMesh.m_numTriangles = geometry.indexes.size();
        indexedMesh.m_numVertices = geometry.vertices.size();
        indexedMesh.m_triangleIndexBase = reinterpret_cast<const unsigned char*>(&geometry.indexes[0]);
        indexedMesh.m_vertexBase = reinterpret_cast<const unsigned char*>(&geometry.vertices[0]);

        // Create bullet collision shape based on the mesh described
        btTriangleIndexVertexArray* meshInterface = new btTriangleIndexVertexArray();
        meshInterface->addIndexedMesh(indexedMesh);
        btBvhTriangleMeshShape* meshShape = new btBvhTriangleMeshShape(meshInterface, true);

        // Create bullet default motionstate (the object is static here)
        // to set the correct position of the collision shape
        btTransform           chunkTransform = btTransform(toBt(glm::quat()), toBt(chunk->getInfo().sphere.position));
        btDefaultMotionState* chunkMotionState = new btDefaultMotionState(chunkTransform);

        // Create the rigidbody with the collision shape
        btRigidBody::btRigidBodyConstructionInfo staticMeshRigidBodyInfo(0, chunkMotionState, meshShape);
        btRigidBody*                             staticRigidMesh = new btRigidBody(staticMeshRigidBodyInfo);

        // Add it to our scene
        _dynamicsWorld->addRigidBody(staticRigidMesh);

        // Keep track of bullet data
        _rigidBodies.push_back(staticRigidMesh);
        _vertexArrayInterfaces.push_back(meshInterface);
    }
}

void BulletContext::createPlayerRigidBody(IPlayer& player)
{
    PlayerBtInfo      playerInfo;
    btMotionState*    motionState = new btMotionState;
    btCollisionShape* collisionShape = new btCapsuleShapeX(2.0f, 3.0f);
    btScalar          mass = player.getWeight();
    btVector3         inertia(0, 0, 0);

    collisionShape->calculateLocalInertia(mass, inertia);
    btRigidBody::btRigidBodyConstructionInfo constructInfo(mass, motionState, collisionShape, inertia);

    playerInfo.body = new btRigidBody(constructInfo);

    // TODO test
    playerInfo.body->setFriction(0.1);
    playerInfo.body->setRollingFriction(0.8);
    playerInfo.player = &player;

    _dynamicsWorld->addRigidBody(playerInfo.body);
    _players.push_back(playerInfo);
}
