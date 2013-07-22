#include <vector>
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>

#include "ModelLoader.hh"
#include "Exceptions/ModelLoaderException.hpp"

ModelLoader::ModelLoader()
{
  _parsers["obj"] = &ModelLoader::loadOBJ;
}

ModelLoader::~ModelLoader() {}

Model* ModelLoader::load(std::string filename)
{
  Model*	model = nullptr;
  std::ifstream	file;
  std::size_t	extensionLength;
  std::string	extension;

  if ((extensionLength = filename.find_last_of(".")) == std::string::npos)
    throw (ModelLoaderException("Invalid file name \'" + filename + "\'"));
  extension = filename.substr(extensionLength + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  if (_parsers[extension] == nullptr)
    throw (ModelLoaderException("Unknown file extension \'" + extension + "\'"));
  file.open(filename);
  if (!file.good())
    throw (ModelLoaderException("Could not open file \'" + filename + "\'"));
  model = ((*this).*(_parsers[extension]))(file);
  file.close();
  return (model);
}

Model* ModelLoader::loadOBJ(std::ifstream& src)
{
  Model*		model = new Model;
  std::string		line;
  std::stringstream	ss;

  glm::vec3		tmpVec3;
  glm::vec2		tmpVec2;
  glm::uvec3		vIdx;

  std::vector<glm::vec3>	vertices;
  std::vector<glm::vec3>	normals;
  std::vector<glm::vec2>	uvs;
  std::vector<glm::uvec3>	indexes;

  while (std::getline(src, line))
  {
    ss.clear();
    ss.str(line);
    ss >> line;
    if (line == "v")
    {
      ss >> tmpVec3[0] >> tmpVec3[1] >> tmpVec3[2];
      vertices.push_back(tmpVec3);
    }
    else if (line == "vn")
    {
      ss >> tmpVec3[0] >> tmpVec3[1] >> tmpVec3[2];
      normals.push_back(tmpVec3);
    }
    else if (line == "vt")
    {
      ss >> tmpVec2[0] >> tmpVec2[1];
      uvs.push_back(tmpVec2);
    }
    else if (line == "f")
    {
      std::string val;
      std::size_t pos;
      /*std::cout << "Parsing line: " << ss.str() << std::endl;
      */for (int i = 0; i < 3; ++i)
      {
	ss >> line;
// 	std::cout << "Parsing token: " << line << std::endl;
	for (int j = 0; j < 3; ++j)
	{
	  val = line.substr(0, line.find_first_of('/'));
	  /*std::cout << "val=" << line << std::endl;
	  */if (!val.empty())
	    vIdx[j] = abs(std::stoul(val)) - 1;
	  else
	    vIdx[j] = 0;
	  if ((pos = line.find_first_of('/')) != std::string::npos)
	    line = line.substr(pos + 1);
	  else
	    line = "";
	}/*
	std::cout << "Parsed IDX: " << vIdx[0] << " " << vIdx[1] << " " << vIdx[2] << std::endl;*/
	indexes.push_back(vIdx);
      }
      /*
      if (ss.str() != "")
      {
	vIdx[1] = vIdx[2];
	ss >> line;
	line = line.substr(0, line.find_first_of('/'));
	vIdx[2] = abs(std::stoul(line)) - 1;
	model->_indexes.push_back(vIdx);
      }
      */
    }
  }

  model->_hasUVs = !(uvs.empty());
  model->_hasNormals = !(normals.empty());

  for (unsigned i = 0; i < indexes.size(); ++i)
  {
    model->_vertices.push_back(vertices[indexes[i][0]]);
    if (model->_hasUVs)
      model->_uvs.push_back(uvs[indexes[i][1]]);
    if (model->_hasNormals)
      model->_normals.push_back(normals[indexes[i][2]]);
  }
  return (model);
}
