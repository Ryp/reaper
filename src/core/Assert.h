////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

REAPER_CORE_API void AssertImpl(const char* file, const char* func, int line, bool condition,
                                const std::string& message = std::string());

#define Assert(...) AssertImpl(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define AssertUnreachable() AssertImpl(__FILE__, __FUNCTION__, __LINE__, false, "unreachable")
