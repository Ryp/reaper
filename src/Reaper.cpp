#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <gli/gli.hpp>

#include "Reaper.hh"
#include "GLHeaders.hpp"
#include "Shader/ShaderProgram.hh"
#include "Model/ModelLoader.hh"
#include "Exceptions/ReaperException.hpp"

Reaper::Reaper(GLContext& context)
  : _context(context)
{}

Reaper::~Reaper() {}

void Reaper::run()
{
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glDepthFunc(GL_LESS);

  ModelLoader loader;
  Model* teapot = loader.load("rc/model/suzanne.obj");

  ShaderProgram shader;
  ShaderObject vs("rc/shader/texture.v.glsl", GL_VERTEX_SHADER);
  ShaderObject fs("rc/shader/texture.f.glsl", GL_FRAGMENT_SHADER);
  ShaderObject gs("rc/shader/texture.g.glsl", GL_GEOMETRY_SHADER);
  shader.attach(vs);
  shader.attach(fs);
  shader.attach(gs);

  shader.link();

  gli::texture2D texture(gli::loadStorageDDS("rc/texture/bricks_diffuse.dds"));
  if (texture.empty())
    throw (ReaperException("could not load texture"));

  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(texture.levels() - 1));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
  glTexStorage2D(GL_TEXTURE_2D,
		 GLint(texture.levels()),
		 GLenum(gli::internal_format(texture.format())),
		 GLsizei(texture.dimensions().x),
		 GLsizei(texture.dimensions().y));
  for (gli::texture2D::size_type Level = 0; Level < texture.levels(); ++Level)
  {
    if (gli::is_compressed(texture.format()))
      glCompressedTexSubImage2D(GL_TEXTURE_2D,
				GLint(Level),
				0, 0,
				GLsizei(texture[Level].dimensions().x),
				GLsizei(texture[Level].dimensions().y),
				GLenum(gli::internal_format(texture.format())),
				GLsizei(texture[Level].size()),
				texture[Level].data());
    else
      glTexSubImage2D(GL_TEXTURE_2D,
		      GLint(Level),
		      0, 0,
		      GLsizei(texture[Level].dimensions().x),
		      GLsizei(texture[Level].dimensions().y),
		      GLenum(gli::external_format(texture.format())),
		      GLenum(gli::type_format(texture.format())),
		      texture[Level].data());
  }

  gli::texture2D textureSpecular(gli::loadStorageDDS("rc/texture/bricks_specular.dds"));
  if (textureSpecular.empty())
    throw (ReaperException("could not load texture"));

  GLuint textureSpecularID;
  glGenTextures(1, &textureSpecularID);
  glBindTexture(GL_TEXTURE_2D, textureSpecularID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(textureSpecular.levels() - 1));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
  glTexStorage2D(GL_TEXTURE_2D,
		 GLint(textureSpecular.levels()),
		 GLenum(gli::internal_format(textureSpecular.format())),
		 GLsizei(textureSpecular.dimensions().x),
		 GLsizei(textureSpecular.dimensions().y));
  for (gli::texture2D::size_type Level = 0; Level < textureSpecular.levels(); ++Level)
  {
    if (gli::is_compressed(textureSpecular.format()))
      glCompressedTexSubImage2D(GL_TEXTURE_2D,
				GLint(Level),
				0, 0,
				GLsizei(textureSpecular[Level].dimensions().x),
				GLsizei(textureSpecular[Level].dimensions().y),
				GLenum(gli::internal_format(textureSpecular.format())),
				GLsizei(textureSpecular[Level].size()),
				textureSpecular[Level].data());
      else
	glTexSubImage2D(GL_TEXTURE_2D,
			GLint(Level),
			0, 0,
		 GLsizei(textureSpecular[Level].dimensions().x),
			GLsizei(textureSpecular[Level].dimensions().y),
			GLenum(gli::external_format(textureSpecular.format())),
			GLenum(gli::type_format(textureSpecular.format())),
			textureSpecular[Level].data());
  }

  glm::vec3 LightPosition(0.0f, 0.5f, 2.0f);
  glm::mat4 Projection = glm::perspective<float>(45.0f, _context.getWindowSize().ratio(), 0.1f, 100.0f);
  glm::mat4 View       = glm::mat4(1.0f);
  glm::mat4 Model      = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
  glm::mat4 MV         = View * Model;
  glm::mat4 MVP        = Projection * MV;
  glm::mat4 Viewport   = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(_context.getWindowSize()[0] / 2.0f, _context.getWindowSize()[1] / 2.0f, 1.0f)), glm::vec3(1.0f, 1.0f, 0.0f));

  glm::vec3 WireframeColor(0.5f, 0.5f, 0.5f);
  int WireframeThickness = 0;

  GLuint vertexbuffer;
  glGenBuffers(1, &vertexbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
  glBufferData(GL_ARRAY_BUFFER, teapot->getVertexBufferSize(), teapot->getVertexBuffer(), GL_STATIC_DRAW);

  GLuint uvbuffer;
  glGenBuffers(1, &uvbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
  glBufferData(GL_ARRAY_BUFFER, teapot->getUVBufferSize(), teapot->getUVBuffer(), GL_STATIC_DRAW);

  GLuint normalbuffer;
  glGenBuffers(1, &normalbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
  glBufferData(GL_ARRAY_BUFFER, teapot->getNormalBufferSize(), teapot->getNormalBuffer(), GL_STATIC_DRAW);

  while (_context.isOpen())
  {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.use();

    Model = glm::rotate(Model, 0.3f, glm::vec3(0, 1, 0));
    View = glm::lookAt(glm::vec3(4,3,3),
		      glm::vec3(0,1,0),
		      glm::vec3(0,1,0));
    MV = View * Model;
    MVP = Projection * MV;

    glUniformMatrix4fv(shader.getUniformLocation("MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(shader.getUniformLocation("MV"), 1, GL_FALSE, &MV[0][0]);
    glUniformMatrix4fv(shader.getUniformLocation("V"), 1, GL_FALSE, &View[0][0]);
    glUniformMatrix4fv(shader.getUniformLocation("ViewportMatrix"), 1, GL_FALSE, &Viewport[0][0]);
    glUniform3fv(shader.getUniformLocation("LighPosition_worldspace"), 1, &LightPosition[0]);

    glUniform3fv(shader.getUniformLocation("WireframeColor"), 1, &WireframeColor[0]);
    glUniform1i(shader.getUniformLocation("WireframeThickness"), WireframeThickness);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(shader.getUniformLocation("TexSampler"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureSpecularID);
    glUniform1i(shader.getUniformLocation("TexSpecularSampler"), 1);

    // 1rst attribute buffer : vertices
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glVertexAttribPointer(
      0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
      3,                  // size
      GL_FLOAT,           // type
      GL_FALSE,           // normalized?
      0,                  // stride
      (void*)0            // array buffer offset
    );
    // 2nd attribute buffer : UVs
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    glVertexAttribPointer(
      1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
      2,                                // size : U+V => 2
      GL_FLOAT,                         // type
      GL_FALSE,                         // normalized?
      0,                                // stride
      (void*)0                          // array buffer offset
    );

    // 3rd attribute buffer : normals
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
    glVertexAttribPointer(
      2,                                // attribute
      3,                                // size
      GL_FLOAT,                         // type
      GL_FALSE,                         // normalized?
      0,                                // stride
      (void*)0                          // array buffer offset
    );

    glDrawArrays(GL_TRIANGLES, 0, teapot->getTriangleCount() * 3);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    _context.swapBuffers();
    glfwPollEvents();
  }
  // Cleanup VBO
  glDeleteBuffers(1, &vertexbuffer);
}
