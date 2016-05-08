# Reaper

Reaper is a small C++14 Engine to test stuff

Originally for OpenGL, now turning into a Tower Defense game.
Vulkan might be used for the rendering also.

## Build

```
mkdir build && pushd build
cmake .. && make
```

Dependencies:
- [GLFW](github.com/glfw/glfw) 3.1+
- [glbinding](github.com/hpicgs/glbinding)
- [moGL](github.com/Ryp/moGL)
- assimp
- libunwind
- OpenEXR
- glm/gli (included)
