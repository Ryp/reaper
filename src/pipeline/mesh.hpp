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

#include <mogl/buffer/buffer.hpp>
#include <mogl/shader/shaderprogram.hpp>

class Model;

class Mesh
{
public:
    enum DrawMode : unsigned int {
        Vertex  = 0x01,
        Normal  = 0x02,
        UV      = 0x04
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
    Model*          _model;
    mogl::Buffer    _vertexBuffer;
    mogl::Buffer    _normalBuffer;
    mogl::Buffer    _uvBuffer;
    mogl::Buffer    _elementBuffer;
    glm::mat4       _modelMatrix;
    GLsizei         _indexesNo;
};

#endif // REAPER_MESH_INCLUDED
