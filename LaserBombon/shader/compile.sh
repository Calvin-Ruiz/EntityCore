#!/bin/bash

for src in $@
do
    dst="$src.spv"
    echo "$src"
    glslc $src -o $dst -I . --target-env="vulkan1.1"
done
