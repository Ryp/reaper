////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_WAVESPAWNERMODULE_INCLUDED
#define REAPER_WAVESPAWNERMODULE_INCLUDED

#include "game/ModuleUpdater.h"

struct WaveSpawnerModuleDescriptor : public ModuleDescriptor {
    u32     accessId;
    u32     unitCount;
    float   delay;
    float   interval;
    std::string unitToSpawn;
};

struct WaveSpawnerModule {
    u32     accessId;
    u32     unitCount;
    float   delay;
    float   interval;
    std::string unitToSpawn;
    float   countdown;
};

class WaveSpawnerUpdater : public ModuleUpdater<WaveSpawnerModule, WaveSpawnerModuleDescriptor>
{
public:
    WaveSpawnerUpdater(AbstractWorldUpdater* worldUpdater) : ModuleUpdater<WaveSpawnerModule, WaveSpawnerModuleDescriptor>(worldUpdater) {}

public:
    void update(float dt);
    void createModule(EntityId id, const WaveSpawnerModuleDescriptor* descriptor) override;
};

#endif // REAPER_WAVESPAWNERMODULE_INCLUDED
