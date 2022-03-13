#ifndef PHYSICS_H_
#define PHYSICS_H_

#include <Track.h>
#include <TrackChunk.h>
#include <cstdint>
#include <fstream>

class IPlayer;
class IPlayerManager;
class IBonusManager;
class IMineManager;
class Track;
struct Movement;
class MovementFactor;

#include "BulletContext.h"

class Physics
{
private:
    float _pLinearFriction;
    float _pQuadFriction;
    float _pHandling;
    float _pBrakesForce;

private:
    IPlayerManager& _playerManager;
    IBonusManager&  _bonusManager;
    IMineManager&   _mineManager;
    Track const*    _track;
    std::ofstream   _csv;
    BulletContext   _btContext;

public:
    Physics(IPlayerManager& playerManager, IBonusManager& bonusManager, IMineManager& mineManager);

    Physics(Physics const&) = delete;
    Physics& operator=(Physics const&) = delete;

    bool updatePlayerPhysics(float dtSecs, Movement& movement, MovementFactor const& factors, IPlayer& player);
    bool isTooFarFromTrack(glm::vec3 const& position, TrackChunk const& nearestChunk) const;
    void respawnOnTrack(Movement& movement, TrackChunk::TrackProjectionInfo const& projection) const;
    bool isOnTrack(const TrackChunk::TrackProjectionInfo& projection);

    bool hasCollision(Movement const& m1, Movement const& m2, float r1, float r2) const;
    void applyCollisionResponse(IPlayer& p1, IPlayer& p2) const;
    void checkBonusCollision(IPlayer& player);
    void checkMineCollision(IPlayer& player);
    void checkPlayersCollision(IPlayer& player);

    void checkFinished(IPlayer& player);

public:
    void        setTrack(Track const* track);
    std::size_t updatePlayersPositions(float deltaTime, bool isMaster);
};

#endif //! PHYSICS_H_
