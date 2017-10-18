#!/bin/bash

CUR=.
BLD=build_ninja

if [ ! -d $BLD ]; then
    cd ..
else
    cd .
fi

cmake -H. -B$BLD -DCMAKE_C_COMPILER=gcc-7 -DCMAKE_CXX_COMPILER=g++-7 -DCPPAN_COMMAND=cppan -DCMAKE_BUILD_TYPE=Debug

cd -

