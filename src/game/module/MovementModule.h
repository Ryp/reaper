////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MOVEMENTMODULE_INCLUDED
#define REAPER_MOVEMENTMODULE_INCLUDED

#include "game/ModuleUpdater.h"
#include "game/pathing/AIPath.h"

#include "PositionModule.h"

struct MovementModuleDescriptor {
    f32 speed;
};

struct MovementModule {
    f32 speed;
    AIPath path;
};

class MovementUpdater : public ModuleUpdater<MovementModule>
{
public:
    MovementUpdater(AbstractWorldUpdater* worldUpdater) : ModuleUpdater<MovementModule>(worldUpdater) {}

public:
    void update(float dt, ModuleAccessor<PositionModule> positionModuleAccessor);
    void createModule(EntityId id, MovementModuleDescriptor const* descriptor);
};

#endif // REAPER_MOVEMENTMODULE_INCLUDED
