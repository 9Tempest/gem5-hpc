# Use Ubuntu 18.04 as the base image
FROM ubuntu:18.04

# Set the maintainer label
LABEL maintainer="lukezhuz@umich.edu"

# Update the package repository
RUN apt-get update

# Install build essentials and other useful tools
RUN apt-get install -y \
    build-essential \
    cmake \
    git \
    vim \
    gdb \
    wget \
    curl \
    gcc \
    g++ \
    mpich 

# Clean up APT cache for a smaller image
RUN apt-get clean && rm -rf /var/lib/apt/lists/*