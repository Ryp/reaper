////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_TEAMMODULE_INCLUDED
#define REAPER_TEAMMODULE_INCLUDED

#include "game/ModuleUpdater.h"

struct TeamModuleDescriptor : public ModuleDescriptor {
    u8 teamId;
};

struct TeamModule {
    u8 teamId;
};

class TeamUpdater : public ModuleUpdater<TeamModule, TeamModuleDescriptor>
{
    using parent_type = ModuleUpdater<TeamModule, TeamModuleDescriptor>;
public:
    TeamUpdater(AbstractWorldUpdater* worldUpdater) : parent_type(worldUpdater) {}

public:
    void createModule(EntityId id, const TeamModuleDescriptor* descriptor) override;
};

#endif // REAPER_TEAMMODULE_INCLUDED
