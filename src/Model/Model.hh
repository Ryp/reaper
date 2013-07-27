#ifndef MODEL_HH
#define MODEL_HH

#include <vector>

#include <glm/glm.hpp>

#include "GLHeaders.hpp"

class Model
{
  friend class ModelLoader;
public:
  Model();
  ~Model();

public:
  void		debugDump();
  unsigned	getTriangleCount();
  void		computeTangents();

public:
  const void*	getVertexBuffer() const;
  unsigned	getVertexBufferSize() const;

  const void*	getUVBuffer() const;
  unsigned	getUVBufferSize() const;

  const void*	getNormalBuffer() const;
  unsigned	getNormalBufferSize() const;

  const void*	getIndexBuffer() const;
  unsigned	getIndexBufferSize() const;

  const void*	getTangentBuffer() const;
  unsigned	getTangentBufferSize() const;

  const void*	getBitangentBuffer() const;
  unsigned	getBitangentBufferSize() const;

private:
  bool				_isIndexed;
  bool				_hasUVs;
  bool				_hasNormals;

private:
  std::vector<glm::vec3>	_vertices;
  std::vector<glm::vec3>	_normals;
  std::vector<glm::vec2>	_uvs;
  std::vector<unsigned int>	_indexes;

  std::vector<glm::vec3>	_tangents;
  std::vector<glm::vec3>	_bitangents;
};

#endif // MODEL_HH
