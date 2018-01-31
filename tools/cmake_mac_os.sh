#!/bin/bash

BDIR=build
GENERATOR=
COMPILER=
BUILD_TYPE=-DCMAKE_BUILD_TYPE=Release

function base() {
#echo "Setup:"
#echo "COMPILER=$COMPILER"
#echo "GENERATOR=$GENERATOR"
#echo "BDIR=$BDIR"

    cmake -H. -B$BDIR \
        -DBISON_EXECUTABLE=/usr/local/Cellar/bison/3.0.4_1/bin/bison \
        -DFLEX_EXECUTABLE=/usr/local/Cellar/flex/2.6.4/bin/flex \
        $COMPILER \
        $GENERATOR \
        $BUILD_TYPE \
        $*
}

function xcode() {
    BDIR="${BDIR}_xcode"
    GENERATOR="-GXcode"
}

function gcc7() {
    BDIR="${BDIR}_gcc7"
    COMPILER="-DCMAKE_C_COMPILER=gcc-7 -DCMAKE_CXX_COMPILER=g++-7"
}

function clang5() {
    BDIR="${BDIR}_clang"
    COMPILER="-DCMAKE_C_COMPILER=clang-5.0 -DCMAKE_CXX_COMPILER=clang-5.0"
}

function ninja() {
    BDIR="${BDIR}_ninja"
    GENERATOR="-GNinja"
}

function debug() {
    BDIR="${BDIR}_debug"
    BUILD_TYPE=-DCMAKE_BUILD_TYPE=Debug
}

function release() {
#BDIR="${BDIR}_release"
    BUILD_TYPE=-DCMAKE_BUILD_TYPE=Release
}

for i in "$@"; do
    eval $i
done

base
