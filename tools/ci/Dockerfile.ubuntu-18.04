FROM ubuntu:18.04
RUN apt-get update && apt-get install locales

# Set the locale
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

# Install dependencies
RUN apt-get update && apt-get install -y                                                                        \
    wget unzip build-essential gcc                                                                              \
    libassimp-dev libvulkan-dev libunwind-dev libx11-xcb-dev

# Use CMake 3.11.3 from upstream until it comes from the package manager
RUN wget https://cmake.org/files/v3.11/cmake-3.11.3-Linux-x86_64.sh                                             \
    && chmod +x cmake-3.11.3-Linux-x86_64.sh                                                                    \
    && yes | ./cmake-3.11.3-Linux-x86_64.sh                                                                     \
    && cp -r cmake-3.11.3-Linux-x86_64/. /usr

# Use SPIRV-Tools from the repo until Ubuntu decides to be a real distro
RUN wget https://github.com/KhronosGroup/SPIRV-Tools/releases/download/master-tot/SPIRV-Tools-master-linux-RelWithDebInfo.zip   \
    && unzip -u -o SPIRV-Tools-master-linux-RelWithDebInfo.zip

# Manually install glslangValidator
RUN wget https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-linux-Release.zip  \
    && unzip -u -o glslang-master-linux-Release.zip
