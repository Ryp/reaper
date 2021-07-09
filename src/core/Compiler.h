////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

// Detect compiler
#if defined(__INTEL_COMPILER) || defined(__ICC)
#    define REAPER_COMPILER_ICC
#elif defined(__GNUC__)
#    define REAPER_COMPILER_GCC
#elif defined(__clang__)
#    define REAPER_COMPILER_CLANG
#elif defined(_MSC_VER)
#    define REAPER_COMPILER_MSVC
#else
#    error "Compiler not recognised!"
#endif

// Define symbol export/import macros
#if defined(REAPER_COMPILER_GCC) || defined(REAPER_COMPILER_CLANG)
#    define REAPER_EXPORT __attribute__((visibility("default")))
#    define REAPER_IMPORT
#elif defined(REAPER_COMPILER_MSVC)
#    define REAPER_EXPORT __declspec(dllexport)
#    define REAPER_IMPORT __declspec(dllimport)
#else
#    error "Could not define import and export macros !"
#endif
