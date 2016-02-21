////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015 Thibault Schueller
/// This file is distributed under the MIT License
///
/// @file mesh.hpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MESH_INCLUDED
#define REAPER_MESH_INCLUDED

#include <glm/detail/type_mat4x4.hpp>

#include "gl.hpp"

#include <mogl/object/buffer/arraybuffer.hpp>
#include <mogl/object/buffer/elementarraybuffer.hpp>
#include <mogl/object/shader/shaderprogram.hpp>

class Model;

class Mesh
{
public:
    enum DrawMode : unsigned int {
        Vertex  = 0x01,
        Normal  = 0x02,
        UV      = 0x04,
        Tangent = 0x08
    };

public:
    Mesh(Model* model);
    ~Mesh() = default;

    Mesh(const Mesh& other) = delete;
    Mesh& operator=(const Mesh& other) = delete;

public:
    void                init();
    void                draw(mogl::ShaderProgram& program, unsigned int mask,
                             unsigned int instances = 0);
    const glm::mat4&    getModelMatrix() const;
    void                setModelMatrix(const glm::mat4& transformation);

public:
    Model*      _model;
    mogl::VBO   _vertexBuffer;
    mogl::VBO   _normalBuffer;
    mogl::VBO   _uvBuffer;
    mogl::VBO   _tanBuffer;
    mogl::VBO   _bitanBuffer;
    mogl::EBO   _elementBuffer;
    glm::mat4   _modelMatrix;
    GLsizei     _indexesNo;
};

#endif // REAPER_MESH_INCLUDED
