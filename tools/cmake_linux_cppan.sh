#!/bin/bash

cmake -H. -Bbuild_cppan_static -DUSE_CPPAN_BUILD=1 -DCMAKE_C_COMPILER=gcc-5 -DCMAKE_CXX_COMPILER=g++-5

