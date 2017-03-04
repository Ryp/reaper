////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "renderer/Scene.h"
#include "renderer/Material.h"

TEST_CASE("Scene")
{
    Scene scene;

    SUBCASE("Add model")
    {
        Model* model = new Model;

        model->material = MaterialId("res/material/text.mat");
        model->mesh = MaterialId("res/model/sphere.obj");

        addModel(scene, model, glm::vec3(0.f), glm::quat());
    }
}

