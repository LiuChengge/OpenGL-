#!/bin/bash
cd "$(dirname "$0")"

# 创建shaders目录
mkdir -p build/shaders

# 复制GLSL文件
cp src/shaders/*.glsl build/shaders/

# 编译到SPIR-V
cd build/shaders
echo "Compiling vertex shader..."
glslangValidator -V -S vert vert.glsl -o vert.spv
echo "Compiling fragment shader..."
glslangValidator -V -S frag frag.glsl -o frag.spv

echo "Shader compilation completed!"
