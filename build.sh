#!/bin/bash
# build script -- i'm too lazy to setup make

###############################################################################################################
# default build configuration: release

deps="deps/lodepng.cpp"
libs="-lassimp -lstdc++fs"
flags="-O3"
forceflags="-Wall -Werror -std=c++17 -Wno-unused-function"
defines=""
files="src/main.cxx src/f3d.cxx src/collision.cxx src/animconv.cxx src/file.cxx"
output="build/modconv2"

# project info

project="modconv 2"
author="red"
builds="release debug\ndebug_all debug_optimizer\ndebug_output"
###############################################################################################################

###############################################################################################################
# other arguments
###############################################################################################################


# clean folder

if [[ $1 == "clean" ]]; then
    rm -r build/
    exit 0
fi

# output help menu

if [[ $1 == "help" ]]; then
    echo "========" $project "========"
    echo "author:" $author
    echo "=========================="
    printf "Build types: $builds\n"
    exit 0
fi

# Compiling occurs here

function compile () {
    echo "[INFO] Building $project..."
    rm *.o &> /dev/null
    rm -r build &> /dev/null
    mkdir build &> /dev/null
    g++ $flags $forceflags $files $files2 $deps -o $output $libs $defines
    if [ $? == 0 ]; then
        echo "[✓] Build succeeded!"
    else
        echo "[X] Build failed!"
    fi
    exit 0
}

###############################################################################################################
# release builds
###############################################################################################################

if [[ $1 == "release" ]]; then
    compile
    exit 0
fi

# fallback

if [[ $1 == "" ]]; then
    compile
    exit 0
fi

if [[ $1 == "conf_release" ]]; then # collision config
    conf_compile
    exit 0
fi

###############################################################################################################
# debug builds (no output)
###############################################################################################################

if [[ $1 == "debug" ]]; then
    flags="-g -fsanitize=address,undefined"
    compile
    exit 0
fi

if [[ $1 == "conf_debug" ]]; then # collision config
    flags="-g -v"
    conf_compile
    exit 0
fi

###############################################################################################################
# debug builds (with output)
###############################################################################################################

if [[ $1 == "debug_all" ]]; then
    flags="-g -v"
    defines="-DDEBUG_ALL"
    compile
    exit 0
fi

if [[ $1 == "debug_output" ]]; then
    flags="-g -v"
    defines="-DDEBUG_OUTPUT"
    compile
    exit 0
fi

if [[ $1 == "debug_optimizer" ]]; then
    flags="-g -v"
    defines="-DDEBUG_OPTIMIZER"
    compile
    exit 0
fi
