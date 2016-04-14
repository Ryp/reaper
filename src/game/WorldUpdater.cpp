////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "WorldUpdater.h"

WorldUpdater::WorldUpdater()
:   _allocator(10000)
{
    _teamUpdater = placementNew<TeamUpdater>(_allocator, this);
    _positionUpdater = placementNew<PositionUpdater>(_allocator, this);
    _damageUpdater = placementNew<DamageUpdater>(_allocator, this);
    _movementUpdater = placementNew<MovementUpdater>(_allocator, this);
    _weaponUpdater = placementNew<WeaponUpdater>(_allocator, this);
    _towerControllerUpdater = placementNew<TowerControllerUpdater>(_allocator, this);
}

WorldUpdater::~WorldUpdater()
{
    _teamUpdater->~TeamUpdater();
    _positionUpdater->~PositionUpdater();
    _damageUpdater->~DamageUpdater();
    _movementUpdater->~MovementUpdater();
    _weaponUpdater->~WeaponUpdater();
    _towerControllerUpdater->~TowerControllerUpdater();
}

void WorldUpdater::updateModules(float dt)
{
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
    towerTeamDesc.teamId = 2;

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

void WorldUpdater::removeEntity(EntityId id)
{
    _teamUpdater->removeModule(id);
    _positionUpdater->removeModule(id);
    _damageUpdater->removeModule(id);
    _movementUpdater->removeModule(id);
    _weaponUpdater->removeModule(id);
    _towerControllerUpdater->removeModule(id);
}
