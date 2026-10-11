FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential g++-12 clang-15 cmake ninja-build git \
    libboost1.74-dev libssl-dev zlib1g-dev ca-certificates valgrind gawk diffutils \
 && rm -rf /var/lib/apt/lists/*
