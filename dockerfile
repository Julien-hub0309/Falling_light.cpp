FROM ubuntu:24.04

RUN apt update && \
    apt install -y \
    git \
    mingw-w64 \
    cmake \
    make \
    g++

WORKDIR /src