////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "ShaderModules.h"

#include <common/Log.h>
#include <common/ReaperRoot.h>

#include <core/Assert.h>
#include <core/fs/FileLoading.h>

#include <filesystem>

namespace Reaper
{
void create_shader_modules(ShaderModules& shader_modules, ReaperRoot& root)
{
    std::filesystem::path shader_folder("build/shader/");

    for (const auto& entry : std::filesystem::recursive_directory_iterator(shader_folder))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".spv")
        {
            std::string       file_path = entry.path().string();
            std::string       file_name = std::filesystem::relative(entry.path(), shader_folder).generic_string();
            std::vector<char> file_contents = readWholeFile(file_path);

            std::size_t offset_bytes = shader_modules.code.size();
            std::size_t size_bytes = file_contents.size();

            Assert(size_bytes % 4 == 0);

            shader_modules.code.insert(shader_modules.code.end(), file_contents.begin(), file_contents.end());
            shader_modules.code_map[file_name] = ShaderModules::CodeSpan{offset_bytes, size_bytes};

            log_debug(root, "shader: loaded SPIR-V file '{}' ({} bytes)", file_name, size_bytes);
        }
    }
}

void destroy_shader_modules(ShaderModules& shader_modules)
{
    shader_modules = {};
}

std::span<const u32> get_spirv_shader_module(const ShaderModules& shader_modules, const char* file_name)
{
    auto it = shader_modules.code_map.find(file_name);
    Assert(it != shader_modules.code_map.end());

    const ShaderModules::CodeSpan& span = it->second;
    const u32*                     ptr = reinterpret_cast<const u32*>(shader_modules.code.data() + span.offset_bytes);

    return std::span<const u32>(ptr, span.size_bytes / sizeof(u32));
}
} // namespace Reaper
