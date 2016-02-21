////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2016 Thibault Schueller
/// This file is distributed under the MIT License
///
/// @file cvar.cpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
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
