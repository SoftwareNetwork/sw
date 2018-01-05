#!/bin/bash

cmake -H. -Bbuild_gcc_7 \
    -DBISON_EXECUTABLE=/usr/local/Cellar/bison/3.0.4_1/bin/bison \
    -DCMAKE_C_COMPILER=gcc-7 \
    -DCMAKE_CXX_COMPILER=g++-7 \
    $*

