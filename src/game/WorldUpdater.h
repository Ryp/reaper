////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_WORLDUPDATER_INCLUDED
#define REAPER_WORLDUPDATER_INCLUDED

#include <memory>
#include <queue>

#include "ModuleUpdater.h"

#include "core/memory/StackAllocator.h"

#include "module/TeamModule.h"
#include "module/PositionModule.h"
#include "module/DamageModule.h"
#include "module/MovementModule.h"
#include "module/WeaponModule.h"
#include "module/TowerControllerModule.h"

class AbstractWorldUpdater
{
public:
    virtual ~AbstractWorldUpdater() {}
    virtual void notifyRemoveEntity(EntityId id) = 0;
};

class WorldUpdater : public AbstractWorldUpdater
{
public:
    WorldUpdater();
    ~WorldUpdater();

public:
    void updateModules(float dt);
    void load();
    void notifyRemoveEntity(EntityId id);

private:
    void removeEntity(EntityId id);

private:
    StackAllocator          _allocator;
    std::queue<EntityId>    _idToRemove;

private:
    TeamUpdater*            _teamUpdater;
    PositionUpdater*        _positionUpdater;
    DamageUpdater*          _damageUpdater;
    MovementUpdater*        _movementUpdater;
    WeaponUpdater*          _weaponUpdater;
    TowerControllerUpdater* _towerControllerUpdater;
};

#endif // REAPER_WORLDUPDATER_INCLUDED
