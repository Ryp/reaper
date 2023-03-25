////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GenericController.h"
#include "InputExport.h"

namespace Reaper
{
struct REAPER_INPUT_API LinuxController
{
    int fd;
};

REAPER_INPUT_API LinuxController create_controller(const char* device_path, GenericControllerState& start_state);
REAPER_INPUT_API void            destroy_controller(LinuxController& controller);

REAPER_INPUT_API GenericControllerState update_controller_state(LinuxController&              controller,
                                                                const GenericControllerState& last_state);
} // namespace Reaper
