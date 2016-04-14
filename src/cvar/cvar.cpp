////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "cvar.hpp"

#include <iostream>

namespace reaper
{
    CVar::CVar(const std::string& name, int flags)
    :   _name(name),
        _flags(flags)
    {

    }
}
