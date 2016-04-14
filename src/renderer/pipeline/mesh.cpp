////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "mesh.hpp"

#include "renderer/resource/Model.hh"

Mesh::Mesh(Model* model)
:   _model(model),
    _indexesNo(_model->getIndexBufferSize())
{}

void Mesh::init()
{
//     _model->debugDump();
    _vertexBuffer.setData(_model->getVertexBufferSize(), _model->getVertexBuffer(), GL_STATIC_DRAW);
    _normalBuffer.setData(_model->getNormalBufferSize(), _model->getNormalBuffer(), GL_STATIC_DRAW);
    _uvBuffer.setData(_model->getUVBufferSize(), _model->getUVBuffer(), GL_STATIC_DRAW);
    _tanBuffer.setData(_model->getTangentBufferSize(), _model->getTangentBuffer(), GL_STATIC_DRAW);
    _bitanBuffer.setData(_model->getBitangentBufferSize(), _model->getBitangentBuffer(), GL_STATIC_DRAW);
    _elementBuffer.setData(_model->getIndexBufferSize(), _model->getIndexBuffer(), GL_STATIC_DRAW);
}

void Mesh::draw(mogl::ShaderProgram& program, unsigned int mask, unsigned int instances)
{
//     vao.bind();
//     if (mask & Mesh::Vertex)
//     {
//         vao.enableAttrib(0);
//         vao.setVertexBuffer(0, _vertexBuffer.getHandle(), 0, 0);
//         vao.setAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 12);
//         vao.setAttribBinding(0, 0);
//     }
//     if (mask & Mesh::Normal)
//     {
//         vao.enableAttrib(1);
//         vao.setVertexBuffer(1, _normalBuffer.getHandle(), 0, 0);
//         vao.setAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 12);
//         vao.setAttribBinding(1, 1);
//     }
//     if (mask & Mesh::UV)
//     {
//         vao.enableAttrib(2);
//         vao.setVertexBuffer(2, _uvBuffer.getHandle(), 0, 0);
//         vao.setAttribFormat(2, 2, GL_FLOAT, GL_FALSE, 8);
//         vao.setAttribBinding(2, 2);
//     }
//     vao.setElementBuffer(_elementBuffer.getHandle());
//
//     glDrawElements(GL_TRIANGLES, _indexesNo, GL_UNSIGNED_INT, nullptr);
//
//     vao.disableAttrib(0);
//     vao.disableAttrib(1);
//     vao.disableAttrib(2);

    if (mask & Mesh::Vertex)
    {
        glEnableVertexAttribArray(0);
        _vertexBuffer.bind();
        program.setVertexAttribPointer(0, 3, GL_FLOAT);
    }
    if (mask & Mesh::Normal)
    {
        glEnableVertexAttribArray(1);
        _normalBuffer.bind();
        program.setVertexAttribPointer(1, 3, GL_FLOAT);
    }
    if (mask & Mesh::UV)
    {
        glEnableVertexAttribArray(2);
        _uvBuffer.bind();
        program.setVertexAttribPointer(2, 2, GL_FLOAT);
    }
    if (mask & Mesh::Tangent)
    {
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);
        _tanBuffer.bind();
        program.setVertexAttribPointer(3, 3, GL_FLOAT);
        _bitanBuffer.bind();
        program.setVertexAttribPointer(4, 3, GL_FLOAT);
    }
    _elementBuffer.bind();

    if (instances)
        glDrawElementsInstanced(GL_TRIANGLES, _indexesNo, GL_UNSIGNED_INT, nullptr, instances);
    else
        glDrawElements(GL_TRIANGLES, _indexesNo, GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
}

const glm::mat4& Mesh::getModelMatrix() const
{
    return _modelMatrix;
}

void Mesh::setModelMatrix(const glm::mat4& transformation)
{
    _modelMatrix = transformation;
}
