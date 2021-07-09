////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Scene.h"

Node* addNode(Scene& scene, glm::vec3 position, glm::quat orientation, u32 parent)
{
    Assert(parent < scene.nodes.size(), "parent node out is of bounds");

    Node newNode;
    newNode.position = position;
    newNode.orientation = orientation;
    newNode.parent = parent;

    scene.nodes.push_back(newNode);
    return &scene.nodes.back();
}

Node* addModel(Scene& scene, Model* model, glm::vec3 position, glm::quat orientation, u32 parent)
{
    Node* node = addNode(scene, position, orientation, parent);

    Assert(model != nullptr);
    Assert(node != nullptr);

    node->type = NodeType::Model;
    node->content = model;

    return node;
}
