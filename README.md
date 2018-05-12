# Reaper

Reaper is a small C++14 Engine to test stuff.

## Master status

| Service | System | Compiler | Status |
| ------- | ------ | -------- | ------ |
| [Travis CI](https://travis-ci.org/Ryp/Reaper)| Ubuntu 18.04 64 bits | GCC 7.0 | [![Travis CI](https://travis-ci.org/Ryp/Reaper.svg?branch=master)](https://travis-ci.org/Ryp/Reaper)
| [AppVeyor](https://ci.appveyor.com/project/Ryp/reaper)| Windows 64 bits | Visual Studio 2015 | [![AppVeyor](https://ci.appveyor.com/api/projects/status/hfvvd697p8c6m2or/branch/master?svg=true)](https://ci.appveyor.com/project/Ryp/reaper)

## Build

```sh
$ git submodule update --init
$ cmake -H. -B./build
$ cmake --build build
```

## Dependencies

- Vulkan
- Assimp
- glslangValidator
- Linux-specific:
    - libunwind
    - libxcb
