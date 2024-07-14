#!/bin/sh

apt update && \
  apt install -y \
    build-essential \
    manpages-dev \
    automake \
    gdb \
    netcat-traditional

