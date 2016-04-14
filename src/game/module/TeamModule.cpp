////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TeamModule.h"

void TeamUpdater::createModule(EntityId id, const TeamModuleDescriptor* descriptor)
{
    TeamModule module;
    module.teamId = descriptor->teamId;

    addModule(id, module);
}
