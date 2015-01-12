////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015 Thibault Schueller
/// This file is distributed under the MIT License
///
/// @file mesh.cpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
////////////////////////////////////////////////////////////////////////////////

#include "mesh.hpp"

#include "resource/Model.hh"

Mesh::Mesh(Model* model)
:   _model(model),
    _vertexBuffer(GL_ARRAY_BUFFER),
    _normalBuffer(GL_ARRAY_BUFFER),
    _uvBuffer(GL_ARRAY_BUFFER),
    _elementBuffer(GL_ELEMENT_ARRAY_BUFFER),
    _indexesNo(_model->getIndexBufferSize())
{}

void Mesh::init()
{
    _model->debugDump();
    _vertexBuffer.bind();
    _vertexBuffer.setData(_model->getVertexBufferSize(), _model->getVertexBuffer(), GL_STATIC_DRAW);
    _normalBuffer.bind();
    _normalBuffer.setData(_model->getNormalBufferSize(), _model->getNormalBuffer(), GL_STATIC_DRAW);
    _uvBuffer.bind();
    _uvBuffer.setData(_model->getUVBufferSize(), _model->getUVBuffer(), GL_STATIC_DRAW);
    _elementBuffer.bind();
    _elementBuffer.setData(_model->getIndexBufferSize(), _model->getIndexBuffer(), GL_STATIC_DRAW);
}

void Mesh::draw(mogl::ShaderProgram& shader, unsigned int mask)
{
    if (mask & Mesh::Vertex)
    {
        glEnableVertexAttribArray(0);
        _vertexBuffer.bind();
        shader.setVertexAttribPointer(0, 3, GL_FLOAT);
    }
    if (mask & Mesh::Normal)
    {
        glEnableVertexAttribArray(1);
        _normalBuffer.bind();
        shader.setVertexAttribPointer(1, 3, GL_FLOAT);
    }
    if (mask & Mesh::UV)
    {
        glEnableVertexAttribArray(2);
        _uvBuffer.bind();
        shader.setVertexAttribPointer(2, 2, GL_FLOAT);
    }
    _elementBuffer.bind();
    glDrawElements(GL_TRIANGLES, _indexesNo, GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}

const glm::mat4& Mesh::getModelMatrix() const
{
    return _modelMatrix;
}

void Mesh::setModelMatrix(const glm::mat4& transformation)
{
    _modelMatrix = transformation;
}
