#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/gtx/string_cast.hpp>

#include "Bonus/Bonus.h"
#include "Bonus/Effect.h"
#include "Bonus/IBonusManager.h"
#include "Bonus/IMineManager.h"
#include "Bonus/Mine.h"
#include "OSUtils.hpp"
#include "Physics/BulletUtils.h"
#include "Physics/Physics.h"
#include "Player/IPlayer.h"
#include "Player/IPlayerManager.h"
#include "TrackChunk.h"

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

Physics::Physics(IPlayerManager& playerManager, IBonusManager& bonusManager, IMineManager& mineManager)
    : _playerManager(playerManager)
    , _bonusManager(bonusManager)
    , _mineManager(mineManager)
    , _track(nullptr)
    , _csv(getConfigPath("proj.csv", "SplineSonic"))
{
    // Physics tweakables
    _pLinearFriction = 4.0f;
    _pQuadFriction = 0.01f;
    _pHandling = 30.0f;
    _pBrakesForce = 30.0f;

    _csv << "SplineParam,Error" << std::endl;
}

static inline glm::vec3 integrateTrapezoidal(const glm::vec3& start, const glm::vec3& end, float interval)
{
    return (start + end) * interval * 0.5f;
}

bool Physics::updatePlayerPhysics(float dtSecs, Movement& movement, MovementFactor const& factors, IPlayer& player)
{
    glm::vec3   forces;
    TrackChunk* nearestChunk = _track->getNearestChunk(movement.position);
    auto        chunkNfo = nearestChunk->getInfo();
    auto        projection = nearestChunk->projectPosition(movement.position);
    glm::vec3   shipUp;
    glm::vec3   shipFwd;

    //     _btContext.update(dtSecs, player);
    //     return false;

    shipUp = projection.orientation * glm::up();
    shipFwd = movement.orientation * glm::forward();
    shipFwd = glm::normalize(shipFwd - glm::proj(shipFwd, shipUp)); // Ship Fwd in on slice plane

    // Re-eval ship orientation
    movement.orientation = glm::quat(glm::mat3(shipFwd, shipUp, glm::cross(shipFwd, shipUp)));

    // change steer angle
    steer(dtSecs, movement, factors.getTurnFactor());

    float param = static_cast<float>(nearestChunk->getId()) + projection.splineParam;
    float error = glm::abs(projection.relativePosition.x);

    _csv << param << ',' << error << std::endl;

    // sum all forces
    // forces += - shipUp * 98.331f; // 10x earth gravity
    forces += -shipUp * 450.0f;                                               // lot of times earth gravity
    forces += shipFwd * factors.getThrustFactor() * player.getAcceleration(); // Engine thrust
    //     forces += shipFwd * player.getAcceleration(); // Engine thrust
    forces += -glm::proj(movement.speed, shipFwd) * factors.getBrakeFactor() * _pBrakesForce; // Brakes force
    forces += -glm::proj(movement.speed, glm::cross(shipFwd, shipUp)) * _pHandling;           // Handling term
    forces += -movement.speed * _pLinearFriction;                                             // Linear friction term
    forces += -movement.speed * glm::length(movement.speed) * _pQuadFriction;                 // Quadratic friction term

    // Progressive repelling force that pushes the ship downwards
    float upSpeed = glm::length(movement.speed - glm::proj(movement.speed, shipUp));
    if (projection.relativePosition.y > 2.0f && upSpeed > 0.0f)
        forces += movement.orientation * (glm::vec3(0.0f, 2.0f - upSpeed, 0.0f) * 10.0f);

    forces *= 1.0f / player.getWeight();

    // integrate on time interval
    //    movement.speed += forces * dtSecs;
    glm::vec3 newSpeed = movement.speed + forces * dtSecs;

    if (isOnTrack(projection))
    {
        glm::vec3   nextPos = movement.position + integrateTrapezoidal(movement.speed, newSpeed, dtSecs);
        TrackChunk* nextChunk = _track->getNearestChunk(nextPos);
        auto        nextProj = nextChunk->projectPosition(nextPos);

        static_cast<void>(nextProj);
        glm::vec3 posProj = glm::proj(projection.trackSnapPosition, shipUp) - glm::proj(nextPos, shipUp);
        if (glm::dot(shipUp, posProj + shipUp) > 0.0f)
            newSpeed += (posProj + shipUp) * (1.0f / dtSecs);
        newSpeed = newSpeed - glm::proj(newSpeed, glm::cross(shipFwd, shipUp));
    }

    movement.position += integrateTrapezoidal(movement.speed, newSpeed, dtSecs);
    movement.speed = newSpeed;

    if (isTooFarFromTrack(movement.position, *nearestChunk))
    {
        TrackChunk* lastChunk = _track->getChunks().front();
        if (nearestChunk == lastChunk)
            projection = _track->getChunk(lastChunk->getId() - 1)->projectPosition(0.25);

        respawnOnTrack(movement, projection);
        return true;
    }
    return false;
}

bool Physics::isTooFarFromTrack(glm::vec3 const& position, TrackChunk const& nearestChunk) const
{
    auto chunkNfo = nearestChunk.getInfo();
    auto projection = nearestChunk.projectPosition(position);

    return (glm::distance(projection.trackSnapPosition, position) > chunkNfo.sphere.radius * 0.6f);
}

void Physics::respawnOnTrack(Movement& movement, TrackChunk::TrackProjectionInfo const& projection) const
{
    bool  notGood = true;
    float factor = 0.0f;

    movement.speed = glm::vec3();
    movement.orientation = projection.orientation;

    while (notGood)
    {
        movement.position = projection.trackSnapPosition + projection.orientation * glm::vec3(factor, 1.0f, 0.0f);
        factor += 1.5f;
        notGood = false;
        _playerManager.applyForAllAlreadyLocked([&](IPlayer& other) {
            if (&movement != &other.getMovement()
                && hasCollision(movement, other.getMovement(), 1.5f * Ship::Radius, other.getRadius()))
                notGood = true;
        });
    }
}

bool Physics::isOnTrack(const TrackChunk::TrackProjectionInfo& projection)
{
    if (projection.relativePosition.y > 0.0f && projection.relativePosition.y < 2.0f
        && projection.relativePosition.z - Ship::Radius / 2.0f < projection.sectionWidth * 0.5f
        && projection.relativePosition.z + Ship::Radius / 2.0f > -projection.sectionWidth * 0.5f)
        return true;
    else
        return false;
}

bool Physics::hasCollision(Movement const& m1, Movement const& m2, float r1, float r2) const
{
    return glm::length(m1.position - m2.position) < (r1 + r2);
}

void Physics::applyCollisionResponse(IPlayer& p1, IPlayer& p2) const
{
    glm::vec3 pos1 = p1.getMovement().position;
    glm::vec3 pos2 = p2.getMovement().position;
    glm::vec3 x = pos1 - pos2;

    x = glm::normalize(x);

    glm::vec3 v1 = p1.getMovement().speed;
    glm::vec3 v1x = x * glm::dot(x, v1);
    glm::vec3 v1y = v1 - v1x;
    float     m1 = p1.getWeight();

    x *= -1.0f;

    glm::vec3 v2 = p2.getMovement().speed;
    glm::vec3 v2x = x * glm::dot(x, v2);
    glm::vec3 v2y = v2 - v2x;
    float     m2 = p2.getWeight();

    if (p2.hasEffect(IEffect::SHIELD))
        p1.getMovement().speed = (v1x * (m1 - m2) / (m1 + m2)) + (v2x * (2 * m2) / (m1 + m2)) + v1y;
    if (!p1.hasEffect(IEffect::SHIELD))
        p2.getMovement().speed = (v1x * (2 * m1) / (m1 + m2)) + (v2x * (m2 - m1) / (m1 + m2)) + v2y;
    // TODO: dirty, use bullet for collision and stuff
}

void Physics::checkBonusCollision(IPlayer& player)
{
    IBonus* bonus = nullptr;

    _bonusManager.applyForAll([&](IBonus& ibonus) {
        if (bonus == nullptr && hasCollision(player.getMovement(), ibonus.getMovement(), Ship::Radius, Bonus::Radius))
            bonus = &ibonus;
    });

    if (bonus != nullptr)
    {
        _bonusManager.removeBonus(bonus->getId(), false);
        _playerManager.pickedUpBonus(player, *bonus);
    }
}

void Physics::checkMineCollision(IPlayer& player)
{
    Mine* mine = nullptr;

    _mineManager.applyForAll([&](Mine& m) {
        if (mine == nullptr && hasCollision(player.getMovement(), m.getMovement(), Ship::Radius, Mine::Radius))
            mine = &m;
    });

    if (mine != nullptr)
    {
        _playerManager.collidedWithMine(player, *mine);
        _mineManager.removeMine(mine->getId(), true);
    }
}

void Physics::checkPlayersCollision(IPlayer& player)
{
    _playerManager.applyForAllAlreadyLocked([&](IPlayer& other) {
        if (&player != &other
            && hasCollision(player.getMovement(), other.getMovement(), player.getRadius(), other.getRadius()))
        {
            player.addEffect(new Effect(IEffect::COLLIDED, 0.5f));
            other.addEffect(new Effect(IEffect::COLLIDED, 0.5));
            _playerManager.playersCollided(player, other);
            applyCollisionResponse(player, other);
        }
    });
}

void Physics::setTrack(Track const* track)
{
    _track = track;
    _btContext.buildTrackCollisionShape(*track);
}

void Physics::checkFinished(IPlayer& player)
{
    auto              position = player.getMovement().position;
    TrackChunk const* currentChunk = _track->getNearestChunk(position);

    if (currentChunk == _track->getChunks().front())
    {
        auto projection = currentChunk->projectPosition(position);

        if (isOnTrack(projection))
            _playerManager.finish(player);
    }
}

size_t Physics::updatePlayersPositions(float deltaTime, bool isMaster)
{
    if (_track == nullptr || _track->getChunks().size() == 0)
        return 0;

    size_t nbrPlayerStillPlaying = 0;

    _playerManager.applyForAll([&](IPlayer& player) {
        if (!player.isFinished() && player.isAlive())
        {
            Movement lastPosition = player.getMovement();
            if (updatePlayerPhysics(deltaTime, player.getMovement(), player.getMovementFactor(), player))
                _playerManager.hasFallen(player, lastPosition);

            // if (isMaster) // TODO: when we have the right events
            checkFinished(player);

            if (!player.isFinished())
            {
                if (isMaster)
                {
                    checkPlayersCollision(player);
                    checkMineCollision(player);
                    if (!player.hasBonus())
                        checkBonusCollision(player);
                }
                ++nbrPlayerStillPlaying;
            }
        }
    });
    return nbrPlayerStillPlaying;
}
