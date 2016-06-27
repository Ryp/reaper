////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_ENTITYDB_INCLUDED
#define REAPER_ENTITYDB_INCLUDED

#include <map>
#include <string>

#include "core/memory/StackAllocator.h"
#include "game/EntityDescriptor.h"

class EntityDb
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

#endif // REAPER_ENTITYDB_INCLUDED
