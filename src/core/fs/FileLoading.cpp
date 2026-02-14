////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FileLoading.h"

#include "core/Assert.h"

#include "fmt/format.h"
#include <fstream>

std::vector<char> readWholeFile(const std::string& file_path)
{
    std::ifstream ifs(file_path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

    Assert(ifs.is_open(), fmt::format("could not open file {}", file_path));

    std::ifstream::pos_type fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<char> bytes(fileSize);
    ifs.read(&bytes[0], fileSize);

    return bytes;
}
