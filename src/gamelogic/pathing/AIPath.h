////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_AIPATH_INCLUDED
#define REAPER_AIPATH_INCLUDED

#include <vector>

#include <glm/vec2.hpp>

struct Segment {
    glm::vec2 origin;
    glm::vec2 end;
};

using AIPath = std::vector<Segment>;

#endif // REAPER_AIPATH_INCLUDED
