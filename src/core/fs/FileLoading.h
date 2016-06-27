////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_FILELOADING_INCLUDED
#define REAPER_FILELOADING_INCLUDED

#include <string>
#include <vector>

REAPER_CORE_API std::vector<char> readWholeFile(const std::string& fileName);

#endif // REAPER_FILELOADING_INCLUDED
