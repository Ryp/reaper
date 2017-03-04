////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MOVEMENTMODULE_INCLUDED
#define REAPER_MOVEMENTMODULE_INCLUDED

#include "gamelogic/ModuleUpdater.h"
#include "gamelogic/pathing/AIPath.h"
#include "gamelogic/map/MapInfo.h"

#include "PositionModule.h"

struct MovementModuleDescriptor : public ModuleDescriptor {
    f32 speed;
};

struct MovementModule {
    f32 speed;
    AIPath path;
};

class MovementUpdater : public ModuleUpdater<MovementModule, MovementModuleDescriptor>
{
    using parent_type = ModuleUpdater<MovementModule, MovementModuleDescriptor>;
public:
    MovementUpdater(AbstractWorldUpdater* worldUpdater) : parent_type(worldUpdater) {}

public:
    void update(float dt, ModuleAccessor<PositionModule> positionModuleAccessor);
    void createModule(EntityId id, const MovementModuleDescriptor* descriptor) override;

public:
    static void buildAIPath(MovementModule* movementModule, const TDPath& path);
};

#endif // REAPER_MOVEMENTMODULE_INCLUDED
