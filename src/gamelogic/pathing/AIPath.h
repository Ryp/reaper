////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include <glm/vec2.hpp>

struct Segment
{
    glm::vec2 origin;
    glm::vec2 end;
};

using AIPath = std::vector<Segment>;
