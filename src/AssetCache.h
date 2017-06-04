////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#include "common/ReaperRoot.h"

struct AssetCache
{
    const std::string m_basePath;

    AssetCache(const std::string& basePath);
};

void load_pak_to_asset_cache(ReaperRoot& root, AssetCache& assetCache, const std::string& filename);

