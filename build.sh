#!/bin/sh

mkdir build -p && cd build && cmake -DCMAKE_INSTALL_PREFIX:PATH=$(pwd) -DCMAKE_BUILD_TYPE=Debug .. && make -j7 && make install && cd examples && make all install
