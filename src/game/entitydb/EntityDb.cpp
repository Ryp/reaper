////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "EntityDb.h"

#include "game/module/TeamModule.h"
#include "game/module/PositionModule.h"
#include "game/module/DamageModule.h"
#include "game/module/MovementModule.h"
#include "game/module/PathModule.h"
#include "game/module/WaveSpawnerModule.h"
#include "game/module/WeaponModule.h"
#include "game/module/TowerControllerModule.h"

EntityDb::EntityDb()
:   _allocator(10000)
{
}

EntityDb::~EntityDb()
{
}

void EntityDb::load()
{
    {
        EntityDescriptor towerEntity;

        TeamModuleDescriptor* towerTeamDesc = placementNew<TeamModuleDescriptor>(_allocator);
        towerTeamDesc->teamId = 1;


        PositionModuleDescriptor* positionDesc = placementNew<PositionModuleDescriptor>(_allocator);
        positionDesc->position = glm::vec3(0.f);
        positionDesc->orientation = glm::vec2(0.f);

        WeaponModuleDescriptor* weaponDesc = placementNew<WeaponModuleDescriptor>(_allocator);
        weaponDesc->damage = 20;
        weaponDesc->rate = 4.f;

        TowerControllerModuleDescriptor* towerControllerDesc = placementNew<TowerControllerModuleDescriptor>(_allocator);
        towerControllerDesc->rangeMin = 0.1f;
        towerControllerDesc->rangeMax = 10.f;
        towerControllerDesc->rotationSpeed = 1.5f;

        towerEntity["TeamModuleDescriptor"] = towerTeamDesc;
        towerEntity["PositionModuleDescriptor"] = positionDesc;
        towerEntity["WeaponModuleDescriptor"] = weaponDesc;
        towerEntity["TowerControllerModuleDescriptor"] = towerControllerDesc;
        _entityDescriptors["tower"] = towerEntity;
    }

    {
        EntityDescriptor enemyEntity;

        TeamModuleDescriptor* targetTeamDesc = placementNew<TeamModuleDescriptor>(_allocator);
        targetTeamDesc->teamId = 2;

        PositionModuleDescriptor* positionTargetDesc = placementNew<PositionModuleDescriptor>(_allocator);
        positionTargetDesc->position = glm::vec3(1.f, 1.f, 1.f);
        positionTargetDesc->orientation = glm::vec2(0.f);

        PathModuleDescriptor* pathTargetDesc = placementNew<PathModuleDescriptor>(_allocator);

        MovementModuleDescriptor* targetMovementDesc = placementNew<MovementModuleDescriptor>(_allocator);
        targetMovementDesc->speed = 0.2f;

        DamageModuleDescriptor* damageTargetDesc = placementNew<DamageModuleDescriptor>(_allocator);
        damageTargetDesc->maxHealth = 500;

        enemyEntity["TeamModuleDescriptor"] = targetTeamDesc;
        enemyEntity["PositionModuleDescriptor"] = positionTargetDesc;
        enemyEntity["PathModuleDescriptor"] = pathTargetDesc;
        enemyEntity["MovementModuleDescriptor"] = targetMovementDesc;
        enemyEntity["DamageModuleDescriptor"] = damageTargetDesc;
        _entityDescriptors["enemy"] = enemyEntity;
    }

    {
        EntityDescriptor waveEntity;

        WaveSpawnerModuleDescriptor* waveSpawner = placementNew<WaveSpawnerModuleDescriptor>(_allocator);
        waveSpawner->accessId = 0;
        waveSpawner->delay = 1.0f;
        waveSpawner->interval = 2.0f;
        waveSpawner->unitCount = 10;
        waveSpawner->unitToSpawn = "enemy";

        waveEntity["WaveSpawnerModuleDescriptor"] = waveSpawner;
        _entityDescriptors["waveSpawner"] = waveEntity;
    }
}

void EntityDb::unload()
{
}

const EntityDescriptor& EntityDb::getEntityDescriptor(const std::string& entityName)
{
    Assert(_entityDescriptors.count(entityName) > 0, "entity not found in db");
    return _entityDescriptors.at(entityName);
}
