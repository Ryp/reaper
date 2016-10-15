# Reaper

Reaper is a small C++14 Engine to test stuff

Originally for OpenGL, now turning into a Tower Defense game.
Vulkan might be used for the rendering also.

## Build

```
git submodule update --init
mkdir build && pushd build
cmake .. && make
```

Dependencies:
- Vulkan
- libunwind (for linux)
- Assimp
