#!/bin/bash

set -e
set -x

git pull

PROG=sw
EXE=sw.client.sw-1.0.0
GUI=sw.client.sw.gui-0.4.0
COMPILER="-compiler gcc"

function run {
    ARCH=$1
    shift
    PLATFORM=$1
    shift

    DIR=~/Nextcloud/sw/$PLATFORM/$ARCH
    BUILD_TYPE=r # rwdi is too big for github
    CFG=${ARCH}_${BUILD_TYPE}
    OUT=.sw/out/$CFG

    $PROG build -sd -sfc -static -config ${BUILD_TYPE} $COMPILER -platform $ARCH -config-name $CFG $* #-Dwith-gui=true
    mkdir -p $DIR

    REMOTE_DIR=sw/client_sync/$PLATFORM/$ARCH
    ssh sw "mkdir -p $REMOTE_DIR"

    chmod 755 $OUT/$EXE
    7z a sw.7z $OUT/$EXE
    cp sw.7z $DIR/
    scp sw.7z sw:$REMOTE_DIR
    rm sw.7z

    if [ "$ARCH" = "${BASE_PLATFORM}" ]; then
        #sudo rm /usr/local/bin/sw
        sudo cp $OUT/$EXE /usr/local/bin/sw
        sudo chmod 755 /usr/local/bin/sw
        sw setup
    fi

    #return

    #$PROG build -sd -static -config ${BUILD_TYPE} $COMPILER -platform $ARCH -config-name $CFG $* -Dwith-gui=true
    chmod 755 $OUT/$GUI
    7z a swgui.7z $OUT/$GUI
    cp swgui.7z $DIR/
    scp swgui.7z sw:$REMOTE_DIR
    rm swgui.7z
}

BASE_PLATFORM=x86_64
run x86_64 linux $*
