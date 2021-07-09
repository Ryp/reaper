////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Path.h"

namespace Reaper
{
std::string getExecutablePath(const std::string& av0)
{
    std::size_t pos;

    if ((pos = av0.find_last_of('/')) == std::string::npos)
        return std::string("./");
    else
        return av0.substr(0, pos + 1);
}
} // namespace Reaper
