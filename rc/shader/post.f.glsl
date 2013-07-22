#version 420

uniform sampler2D fbo_texture;
uniform float offset;
varying vec2 f_texcoord;

void main(void) {
  vec2 texcoord = f_texcoord;
  texcoord.x += sin(texcoord.y * 4*2*3.14159 + offset) / 1000;
  gl_FragColor = texture2D(fbo_texture, texcoord);
}
