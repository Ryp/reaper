////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FileLoading.h"

#include <fstream>

std::vector<char> readWholeFile(const std::string& fileName)
{
    std::ifstream ifs(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

    Assert(ifs.is_open(), "could not open file");

    std::ifstream::pos_type fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<char> bytes(fileSize);
    ifs.read(&bytes[0], fileSize);

    return bytes;
}
