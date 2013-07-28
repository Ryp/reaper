#include <iostream>
#include "Debug.hh"

void debug::debugMat(const glm::mat4& mat)
{
  std::cout << "[";
  for (int j = 0; j < 4; ++j)
  {
    std::cout << "[";
    for (int i = 0; i < 4; ++i)
      std::cout << "[" << mat[i][j] << "], ";
    std::cout << "]" << std::endl;
  }
  std::cout << "]";
}
