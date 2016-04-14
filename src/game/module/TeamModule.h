////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_TEAMMODULE_INCLUDED
#define REAPER_TEAMMODULE_INCLUDED

#include "game/ModuleUpdater.h"

struct TeamModuleDescriptor {
    u8 teamId;
};

struct TeamModule {
    u8 teamId;
};

class TeamUpdater : public ModuleUpdater<TeamModule>
{
public:
    TeamUpdater(AbstractWorldUpdater* worldUpdater) : ModuleUpdater<TeamModule>(worldUpdater) {}

public:
    void createModule(EntityId id, TeamModuleDescriptor const* descriptor);
};

#endif // REAPER_TEAMMODULE_INCLUDED
