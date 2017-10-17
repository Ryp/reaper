////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

REAPER_CORE_API void AssertImpl(bool condition, const char* file, const char* func, int line,
                                const std::string& message = std::string());

// Macro overloading code (yuck)
// http://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments
// http://stackoverflow.com/questions/9183993/msvc-variadic-macro-expansion

#define REAPER_GLUE(x, y) x y

#define REAPER_RETURN_ARG_COUNT(_1_, _2_, _3_, _4_, _5_, count, ...) count
#define REAPER_EXPAND_ARGS(args) REAPER_RETURN_ARG_COUNT args
#define REAPER_COUNT_ARGS_MAX5(...) REAPER_EXPAND_ARGS((__VA_ARGS__, 5, 4, 3, 2, 1, 0))

#define REAPER_OVERLOAD_MACRO2(name, count) name##count
#define REAPER_OVERLOAD_MACRO1(name, count) REAPER_OVERLOAD_MACRO2(name, count)
#define REAPER_OVERLOAD_MACRO(name, count) REAPER_OVERLOAD_MACRO1(name, count)

#define REAPER_CALL_OVERLOAD(name, ...) \
    REAPER_GLUE(REAPER_OVERLOAD_MACRO(name, REAPER_COUNT_ARGS_MAX5(__VA_ARGS__)), (__VA_ARGS__))

#define Assert1(condition) AssertImpl(condition, __FILE__, __FUNCTION__, __LINE__)
#define Assert2(condition, message) AssertImpl(condition, __FILE__, __FUNCTION__, __LINE__, message)

// Actual macros
#define Assert(...) REAPER_CALL_OVERLOAD(Assert, __VA_ARGS__)
#define AssertUnreachable() AssertImpl(false, __FILE__, __FUNCTION__, __LINE__, "unreachable")
