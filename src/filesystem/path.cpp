////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015 Thibault Schueller
/// This file is distributed under the MIT License
///
/// @file path.cpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
////////////////////////////////////////////////////////////////////////////////

#include "path.hpp"

std::string reaper::getExecutablePath(const std::string& av0)
{
    std::size_t pos;

    if ((pos = av0.find_last_of('/')) == std::string::npos)
        return std::string("./");
    else
        return av0.substr(0, pos + 1);
}
