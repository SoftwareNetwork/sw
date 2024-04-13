#!/bin/bash

set -e
set -x

git pull

PROG=sw
EXE=sw.client.sw-1.0.0
COMPILER="-compiler gcc"

function run {
    ARCH=$1
    shift
    PLATFORM=$1
    shift

    BUILD_TYPE=r # rwdi is too big for github
    CFG=${ARCH}_${BUILD_TYPE}
    OUT=.sw/out/$CFG

    $PROG build -sd -sfc -static -config ${BUILD_TYPE} $COMPILER -platform $ARCH -config-name $CFG $*
    chmod 755 $OUT/$EXE

    if [ "$ARCH" = "${BASE_PLATFORM}" ]; then
        #sudo rm /usr/local/bin/sw
        sudo cp $OUT/$EXE /usr/local/bin/sw
        sudo chmod 755 /usr/local/bin/sw
        sw setup
    fi
}

BASE_PLATFORM=x86_64
run x86_64 linux $*
