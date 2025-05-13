BIN=../bin

CC=gcc
EXE=le_chat_noir
SRC=../src/xcb/xcb_main.c
INCLUDE=../src/
LIBS="-lX11 -lX11-xcb -lm -lxcb -lxcb-xfixes -lxcb-keysyms -lvulkan"
FLAGS="-g -O3 -Wall"

GLSLC=glslc
SHADER_SRC=../src/vulkan/shaders/
SHADER_OUT=$BIN/shaders

# Code analysis
sh analyze.sh
if [ $? -ne 0 ]; then
	exit 1
fi

# Shader compilation
printf "Compiling GLSL...\n"

$GLSLC $SHADER_SRC/world.vert -o $SHADER_OUT/world_vert.spv
if [ $? -ne 0 ]; then
	exit 1
fi
$GLSLC $SHADER_SRC/world.frag -o $SHADER_OUT/world_frag.spv
if [ $? -ne 0 ]; then
	exit 1
fi
$GLSLC $SHADER_SRC/reticle.vert -o $SHADER_OUT/reticle_vert.spv
if [ $? -ne 0 ]; then
	exit 1
fi
$GLSLC $SHADER_SRC/reticle.frag -o $SHADER_OUT/reticle_frag.spv
if [ $? -ne 0 ]; then
	exit 1
fi

# Executable compilation
printf "Compiling executable...\n"

$CC -o $BIN/$EXE $SRC -I $INCLUDE $FLAGS $LIBS

if [ $? -eq 0 ]; then
    printf "Compilation was \033[0;32m\033[1msuccessful\033[0m.\n"
    exit 0
else
	exit 1
fi
