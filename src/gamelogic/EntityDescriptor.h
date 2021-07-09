////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <string>

struct ModuleDescriptor
{
    virtual ~ModuleDescriptor() {}
};

using EntityDescriptor = std::map<std::string, ModuleDescriptor*>;

using EntityId = u32;

constexpr EntityId invalidEId()
{
    return EntityId(0);
}
