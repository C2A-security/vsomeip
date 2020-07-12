#!/bin/sh

mkdir build -p && cd build && rm CMakeCache.txt -f
cmake --cmake-clean-first -DCMAKE_TOOLCHAIN_FILE=CMakeToolchain386.txt -DCMAKE_INSTALL_PREFIX:PATH=$(pwd)/i386 -DCMAKE_BUILD_TYPE=Debug .. # this fails the first time not adding -m32 to linker
cmake -DCMAKE_TOOLCHAIN_FILE=CMakeToolchain386.txt -DCMAKE_INSTALL_PREFIX:PATH=$(pwd)/i386 -DCMAKE_BUILD_TYPE=Debug ..	&& make -j7 && make install && cd examples && make all install VERBOSE=1
