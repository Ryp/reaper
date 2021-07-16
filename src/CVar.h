////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#include "core/BitTricks.h"

namespace Reaper
{
class CVar
{
public:
    enum Flag
    {
        None = 0,
        Bool = bit(0),
        Int = bit(1),
        Float = bit(2),
        String = bit(3)
    };

public:
    CVar(const std::string& name, int flags);
    ~CVar() = default;

    CVar(const CVar& other) = delete;
    CVar& operator=(const CVar& other) = delete;

public:
private:
    const std::string _name;
    int               _flags;
    bool              _valueBool;
    int               _valueInt;
    float             _valueFloat;
    std::string       _valueStr;
};
} // namespace Reaper
