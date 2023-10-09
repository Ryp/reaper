////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MeshExport.h"

#include <fstream>
#include <string>

#include <span>

#include "Mesh.h"

REAPER_MESH_API Mesh load_obj(const std::string& filename);
REAPER_MESH_API void save_obj(std::ostream& output, std::span<const Mesh> meshes);
