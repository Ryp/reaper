FROM ubuntu:20.04
RUN apt-get update && apt-get install locales

# Set the locale
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

# Install dependencies
RUN apt-get update && apt-get install -y                                                                        \
    wget unzip build-essential cmake gcc                                                                        \
    libunwind-dev libx11-xcb-dev spirv-tools glslang-dev
