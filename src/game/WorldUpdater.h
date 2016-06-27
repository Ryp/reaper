////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_WORLDUPDATER_INCLUDED
#define REAPER_WORLDUPDATER_INCLUDED

#include <string>
#include <map>
#include <queue>
#include <memory>

#include "ModuleUpdater.h"

#include "core/memory/StackAllocator.h"
#include "core/memory/UniquePtr.h"

#include "module/TeamModule.h"
#include "module/PositionModule.h"
#include "module/DamageModule.h"
#include "module/MovementModule.h"
#include "module/PathModule.h"
#include "module/WaveSpawnerModule.h"
#include "module/WeaponModule.h"
#include "module/TowerControllerModule.h"

class AbstractWorldUpdater
{
public:
    virtual ~AbstractWorldUpdater() {}
    virtual void notifyRemoveEntity(EntityId id) = 0;
    virtual void notifySpawnEnemy(const std::string& entityName, u32 accessId) = 0;
};

class EntityDb;

class WorldUpdater : public AbstractWorldUpdater
{
    using ModuleCreators = std::map<std::string, AbstractModuleUpdater*>;
public:
    WorldUpdater(EntityDb* entityDb);
    ~WorldUpdater();

public:
    void updateModules(float dt);
    void load();
    void unload();

    void notifyRemoveEntity(EntityId id) override;
    void notifySpawnEnemy(const std::string& entityName, u32 accessId) override;

private:
    EntityId    createEntity(const std::string& entityName);
    void        removeEntity(EntityId id);

private:
    EntityDb*               _entityDb;
    EntityId                _currentId;
    ModuleCreators          _modulesCreators;
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
    UniquePtr<WaveSpawnerUpdater>       _waveSpawnerUpdater;
    UniquePtr<TowerControllerUpdater>   _towerControllerUpdater;
};

#endif // REAPER_WORLDUPDATER_INCLUDED
