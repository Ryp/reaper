////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "WorldUpdater.h"

#include "gamelogic/entitydb/EntityDb.h"
#include "map/MapDescriptor.h"

WorldUpdater::WorldUpdater(EntityDb* entityDb)
    : _entityDb(entityDb)
    , _currentId(0)
    , _allocator(10000)
    , _mapInfo(_allocator)
    , _teamUpdater(_allocator, this)
    , _positionUpdater(_allocator, this)
    , _damageUpdater(_allocator, this)
    , _movementUpdater(_allocator, this)
    , _pathUpdater(_allocator, this, *_mapInfo)
    , _weaponUpdater(_allocator, this)
    , _waveSpawnerUpdater(_allocator, this)
    , _towerControllerUpdater(_allocator, this)
{
    _modulesCreators["TeamModuleDescriptor"] = _teamUpdater.get();
    _modulesCreators["PositionModuleDescriptor"] = _positionUpdater.get();
    _modulesCreators["DamageModuleDescriptor"] = _damageUpdater.get();
    _modulesCreators["MovementModuleDescriptor"] = _movementUpdater.get();
    _modulesCreators["PathModuleDescriptor"] = _pathUpdater.get();
    _modulesCreators["WeaponModuleDescriptor"] = _weaponUpdater.get();
    _modulesCreators["WaveSpawnerModuleDescriptor"] = _waveSpawnerUpdater.get();
    _modulesCreators["TowerControllerModuleDescriptor"] = _towerControllerUpdater.get();
}

WorldUpdater::~WorldUpdater()
{}

void WorldUpdater::updateModules(float dt)
{
    _pathUpdater->update(dt, _movementUpdater->getModuleAccessor(), _positionUpdater->getModuleAccessor());
    _movementUpdater->update(dt, _positionUpdater->getModuleAccessor());
    _positionUpdater->update(dt);
    _weaponUpdater->update(dt);
    _towerControllerUpdater->update(dt, _positionUpdater->getModuleAccessor(), _weaponUpdater->getModuleAccessor(),
                                    _damageUpdater->getModuleAccessor(), _teamUpdater->getModuleAccessor());
    _damageUpdater->update(dt);
    _waveSpawnerUpdater->update(dt);

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

// NOTE: maybe this should be done at the end of a frame
void WorldUpdater::notifySpawnEnemy(const std::string& entityName, u32 accessId)
{
    EntityId                   id = createEntity(entityName);
    ModuleAccessor<PathModule> pathModuleAccessor = _pathUpdater->getModuleAccessor();
    PathModule*                pathModule = pathModuleAccessor[id];

    pathModule->pathId = accessId;
}

void WorldUpdater::load()
{
    {
        // Load map
        MapDescriptor mapDesc;

        mapDesc.dimensions = uvec2(8, 8);

        MapAccess access1;
        MapAccess access2;

        access1.entrance = uvec2(0, 0);
        access1.exit = uvec2(7, 5);
        access2.entrance = uvec2(3, 3);
        access2.exit = uvec2(5, 7);

        mapDesc.accesses.push_back(access1);
        mapDesc.accesses.push_back(access2);

        _mapInfo->load(&mapDesc);
    }

    createEntity("tower");
    createEntity("waveSpawner");
}

void WorldUpdater::unload()
{
    _mapInfo->unload();
}

EntityId WorldUpdater::createEntity(const std::string& entityName)
{
    const EntityDescriptor& entityDescriptor = _entityDb->getEntityDescriptor(entityName);
    const EntityId          id = _currentId;

    for (auto& moduleDescriptor : entityDescriptor)
    {
        Assert(_modulesCreators.count(moduleDescriptor.first) > 0);
        _modulesCreators.at(moduleDescriptor.first)->createModule(id, moduleDescriptor.second);
    }
    ++_currentId;
    return id;
}

void WorldUpdater::removeEntity(EntityId id)
{
    _teamUpdater->removeModule(id);
    _positionUpdater->removeModule(id);
    _damageUpdater->removeModule(id);
    _movementUpdater->removeModule(id);
    _pathUpdater->removeModule(id);
    _weaponUpdater->removeModule(id);
    _waveSpawnerUpdater->removeModule(id);
    _towerControllerUpdater->removeModule(id);
}
