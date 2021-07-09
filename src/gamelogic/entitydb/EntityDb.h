////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <string>

#include "core/memory/StackAllocator.h"
#include "gamelogic/EntityDescriptor.h"

class REAPER_GAMELOGIC_API EntityDb
{
public:
    EntityDb();
    ~EntityDb();

public:
    void load();
    void unload();

    const EntityDescriptor& getEntityDescriptor(const std::string& entityName);

private:
    StackAllocator                          _allocator;
    std::map<std::string, EntityDescriptor> _entityDescriptors;
};
