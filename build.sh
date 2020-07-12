#!/bin/sh

mkdir build -p && cd build && cmake --cmake-clean-first -DCMAKE_INSTALL_PREFIX:PATH=$(pwd) -DCMAKE_BUILD_TYPE=Debug .. && make -j7 && make install && cd examples && make all install VERBOSE=1
