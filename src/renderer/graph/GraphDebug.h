////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "FrameGraph.h"

namespace Reaper::FrameGraph
{
struct FrameGraph;

// Thibault S. (12/02/2018) Very useful graph dumping routine.
// Will dump multiple txt files in a hardcoded location
// (you may need to modify it).
// A script should be available to you containing a makefile
// and the necessary templates to build a png from this data.
REAPER_RENDERER_API
void DumpFrameGraph(const FrameGraph& frameGraph);
} // namespace Reaper::FrameGraph
