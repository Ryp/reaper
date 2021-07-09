////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <vector>

#include "resource/Resource.h"

struct Model
{
    MeshId     mesh;
    MaterialId material;
    // AABB* boundingBox;
};

enum class LightType
{
    Point,
    Spot
};

struct LightSource
{
    glm::vec4 color;
    float     brightness;
    LightType type;
};

enum class NodeType : u8
{
    None = 0,
    Model,
    Light,
    FX
};

using NodeContent = u8;

struct Node
{
    // Absolute minimum
    glm::vec3 position;
    glm::quat orientation;
    u32       parent;

    NodeType type;
    void*    content;
};

struct REAPER_RENDERER_API Scene
{
    std::vector<Node> nodes;
};

REAPER_RENDERER_API Node* addNode(Scene& scene, glm::vec3 position = glm::vec3(0.f),
                                  glm::quat orientation = glm::quat(), u32 parent = 0);
REAPER_RENDERER_API Node* addModel(Scene& scene, Model* model, glm::vec3 position = glm::vec3(0.f),
                                   glm::quat orientation = glm::quat(), u32 parent = 0);
