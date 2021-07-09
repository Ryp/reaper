////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "gamelogic/ModuleUpdater.h"

struct WaveSpawnerModuleDescriptor : public ModuleDescriptor
{
    u32         accessId;
    u32         unitCount;
    float       delay;
    float       interval;
    std::string unitToSpawn;
};

struct WaveSpawnerModule
{
    u32         accessId;
    u32         unitCount;
    float       delay;
    float       interval;
    std::string unitToSpawn;
    float       countdown;
};

class WaveSpawnerUpdater : public ModuleUpdater<WaveSpawnerModule, WaveSpawnerModuleDescriptor>
{
    using parent_type = ModuleUpdater<WaveSpawnerModule, WaveSpawnerModuleDescriptor>;

public:
    WaveSpawnerUpdater(AbstractWorldUpdater* worldUpdater)
        : parent_type(worldUpdater)
    {}

public:
    void update(float dt);
    void createModule(EntityId id, const WaveSpawnerModuleDescriptor* descriptor) override;
};
