////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "WaveSpawnerModule.h"

#include "gamelogic/WorldUpdater.h"

void WaveSpawnerUpdater::update(float dt)
{
    for (auto& it : _modules)
    {
        WaveSpawnerModule& waveSpawner = it.second;
        waveSpawner.countdown = std::max(0.f, waveSpawner.countdown - dt);

        // Spawn unit
        if (waveSpawner.unitCount > 0 && waveSpawner.countdown <= 0.f)
        {
            _worldUpdater->notifySpawnEnemy(waveSpawner.unitToSpawn, waveSpawner.accessId);
            waveSpawner.countdown = waveSpawner.delay;
            --waveSpawner.unitCount;
        }

        // Spawner depleted
        if (waveSpawner.unitCount == 0)
            _worldUpdater->notifyRemoveEntity(it.first);
    }
}

void WaveSpawnerUpdater::createModule(EntityId id, const WaveSpawnerModuleDescriptor* descriptor)
{
    WaveSpawnerModule module;
    module.accessId = descriptor->accessId;
    module.unitCount = descriptor->unitCount;
    module.delay = descriptor->delay;
    module.interval = descriptor->interval;
    module.unitToSpawn = descriptor->unitToSpawn;
    module.countdown = descriptor->delay;

    Assert(module.unitCount > 0);
    Assert(module.delay > 0.f);
    Assert(module.interval > 0.f);
    Assert(!module.unitToSpawn.empty());

    addModule(id, module);
}
