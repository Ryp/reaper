////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SimExport.h"

#include <core/Types.h>

#include <span>

#include <glm/fwd.hpp>

namespace Neptune
{
struct PhysicsSim;

NEPTUNE_SIM_API void sim_start(PhysicsSim* sim);

struct TrackSkeletonNode;
struct ShipInput;

NEPTUNE_SIM_API void sim_update(PhysicsSim& sim, std::span<const TrackSkeletonNode> skeleton_nodes,
                                const ShipInput& input, float dt);
} // namespace Neptune
