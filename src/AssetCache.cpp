////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "AssetCache.h"

#include "common/Log.h"

#include "core/Profile.h"
#include "core/fs/FileLoading.h"

#include <fstream>
#include <sstream>

namespace Reaper
{
AssetCache::AssetCache(const std::string& basePath)
    : m_basePath(basePath)
{}

void load_pak_to_asset_cache(ReaperRoot& root, AssetCache& assetCache, const std::string& filename)
{
    REAPER_PROFILE_SCOPE("Asset", MP_GREEN);

    const std::string pakPath = assetCache.m_basePath + filename;

    log_info(root, "asset: loading pak file, path = {}", pakPath);

    std::ifstream file(pakPath);

    Assert(file.is_open(), "could not open pak file");

    std::string lineStr;
    while (std::getline(file, lineStr))
    {
        if (lineStr.empty())
            continue;

        std::istringstream line(lineStr);
        std::string        type;
        std::string        resourceFilename;

        line >> type;
        line >> resourceFilename;

        Assert(!type.empty(), "empty type");
        Assert(!resourceFilename.empty(), "invalid resource");

        std::vector<char> content = readWholeFile(resourceFilename);

        Assert(!content.empty(), "empty resource");

        log_debug(root, "asset: loaded resource file, type = {}, name = {}", type, resourceFilename);
    }
}
} // namespace Reaper
