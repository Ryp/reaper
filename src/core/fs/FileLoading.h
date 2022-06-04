////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "core/CoreExport.h"

#include <string>
#include <vector>

REAPER_CORE_API std::vector<char> readWholeFile(const std::string& fileName);
