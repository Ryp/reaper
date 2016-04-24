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
#include "core/memory/UniquePtr.h"

#include "module/TeamModule.h"
#include "module/PositionModule.h"
#include "module/DamageModule.h"
#include "module/MovementModule.h"
#include "module/PathModule.h"
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
    void unload();
    void notifyRemoveEntity(EntityId id);

private:
    void removeEntity(EntityId id);

private:
    StackAllocator          _allocator;
    std::queue<EntityId>    _idToRemove;

private:
    UniquePtr<MapInfo>  _mapInfo;

private:
    UniquePtr<TeamUpdater>              _teamUpdater;
    UniquePtr<PositionUpdater>          _positionUpdater;
    UniquePtr<DamageUpdater>            _damageUpdater;
    UniquePtr<MovementUpdater>          _movementUpdater;
    UniquePtr<PathUpdater>              _pathUpdater;
    UniquePtr<WeaponUpdater>            _weaponUpdater;
    UniquePtr<TowerControllerUpdater>   _towerControllerUpdater;
};

#endif // REAPER_WORLDUPDATER_INCLUDED
