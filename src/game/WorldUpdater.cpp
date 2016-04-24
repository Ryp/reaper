////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "WorldUpdater.h"

#include "map/MapDescriptor.h"

WorldUpdater::WorldUpdater()
:   _allocator(10000)
,   _mapInfo(_allocator)
,   _teamUpdater(_allocator, this)
,   _positionUpdater(_allocator, this)
,   _damageUpdater(_allocator, this)
,   _movementUpdater(_allocator, this)
,   _pathUpdater(_allocator, this, *_mapInfo)
,   _weaponUpdater(_allocator, this)
,   _towerControllerUpdater(_allocator, this)
{}

WorldUpdater::~WorldUpdater()
{}

void WorldUpdater::updateModules(float dt)
{
    _pathUpdater->update(dt, _movementUpdater->getModuleAccessor());
    _movementUpdater->update(dt, _positionUpdater->getModuleAccessor());
    _positionUpdater->update(dt);
    _weaponUpdater->update(dt);
    _towerControllerUpdater->update(dt, _positionUpdater->getModuleAccessor(),
                                        _weaponUpdater->getModuleAccessor(),
                                        _damageUpdater->getModuleAccessor(),
                                        _teamUpdater->getModuleAccessor());
    _damageUpdater->update(dt);

    // Process entity removal
    while (!_idToRemove.empty())
    {
        removeEntity(_idToRemove.front());
        _idToRemove.pop();
    }
}

void WorldUpdater::notifyRemoveEntity(EntityId id)
{
    _idToRemove.push(id);
}

void WorldUpdater::load()
{
    {
        // Load map
        MapDescriptor mapDesc;

        mapDesc.dimensions = uvec2(8, 8);

        MapAccess   access1;
        MapAccess   access2;

        access1.entrance = uvec2(0, 0);
        access1.exit = uvec2(7, 5);
        access2.entrance = uvec2(3, 3);
        access2.exit = uvec2(5, 7);

        mapDesc.accesses.push_back(access1);
        mapDesc.accesses.push_back(access2);

        _mapInfo->load(&mapDesc);
    }
    {
        // Load modules
        TeamModuleDescriptor towerTeamDesc;
        towerTeamDesc.teamId = 1;

        PositionModuleDescriptor positionDesc;
        positionDesc.position = glm::vec3(0.f);
        positionDesc.orientation = glm::vec2(0.f);

        WeaponModuleDescriptor weaponDesc;
        weaponDesc.damage = 20;
        weaponDesc.rate = 4.f;

        TowerControllerModuleDescriptor towerControllerDesc;
        towerControllerDesc.range = 10.f;
        towerControllerDesc.rotationSpeed = 1.5f;

        TeamModuleDescriptor targetTeamDesc;
        targetTeamDesc.teamId = 2;

        PositionModuleDescriptor positionTargetDesc;
        positionTargetDesc.position = glm::vec3(1.f, 1.f, 1.f);
        positionTargetDesc.orientation = glm::vec2(0.f);

        MovementModuleDescriptor targetMovementDesc;
        targetMovementDesc.speed = 0.2f;

        DamageModuleDescriptor damageTargetDesc;
        damageTargetDesc.maxHealth = 500;

        _teamUpdater->createModule(1, &towerTeamDesc);
        _positionUpdater->createModule(1, &positionDesc);
        _weaponUpdater->createModule(1, &weaponDesc);
        _towerControllerUpdater->createModule(1, &towerControllerDesc);

        _teamUpdater->createModule(2, &targetTeamDesc);
        _positionUpdater->createModule(2, &positionTargetDesc);
        _movementUpdater->createModule(2, &targetMovementDesc);
        _damageUpdater->createModule(2, &damageTargetDesc);
    }
}

void WorldUpdater::unload()
{
    _mapInfo->unload();
}

void WorldUpdater::removeEntity(EntityId id)
{
    _teamUpdater->removeModule(id);
    _positionUpdater->removeModule(id);
    _damageUpdater->removeModule(id);
    _movementUpdater->removeModule(id);
    _pathUpdater->removeModule(id);
    _weaponUpdater->removeModule(id);
    _towerControllerUpdater->removeModule(id);
}
