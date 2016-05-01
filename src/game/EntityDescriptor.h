////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_ENTITYDESCRIPTOR_INCLUDED
#define REAPER_ENTITYDESCRIPTOR_INCLUDED

#include <map>
#include <string>

struct ModuleDescriptor
{
    virtual ~ModuleDescriptor() {}
};

using EntityDescriptor = std::map<std::string, ModuleDescriptor*>;

using EntityId = u32;

#endif // REAPER_ENTITYDESCRIPTOR_INCLUDED
