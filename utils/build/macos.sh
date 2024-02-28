#!/bin/bash

set -e
set -x

git pull

PROG=sw
EXE=sw.client.sw-1.0.0
GUI=sw.client.sw.gui-0.4.0
COMPILER=

function run {
    ARCH=$1
    shift
    PLATFORM=$1
    shift

    DIR=~/Nextcloud/sw/$PLATFORM/$ARCH
    BUILD_TYPE=r #rwdi is too big for github
    CFG=${ARCH}_${BUILD_TYPE}
    OUT=.sw/out/$CFG

    REMOTE_DIR=sw/client_sync/$PLATFORM/$ARCH
    ssh sw "mkdir -p $REMOTE_DIR"

    $PROG build -sd -static -config ${BUILD_TYPE} $COMPILER -platform $ARCH -config-name $CFG $* -Dwith-gui=true -sfc
    mkdir -p $DIR

    chmod 755 $OUT/$EXE
    7zz a sw.7z $OUT/$EXE
    cp sw.7z $DIR/
    scp sw.7z sw:$REMOTE_DIR
    rm sw.7z

    if [ "$ARCH" = "${BASE_PLATFORM}" ]; then
        sudo rm /usr/local/bin/sw
        sudo cp $OUT/$EXE /usr/local/bin/sw
    fi

    #return

    chmod 755 $OUT/$GUI
    7zz a swgui.7z $OUT/$GUI
    cp swgui.7z $DIR/
    scp swgui.7z sw:$REMOTE_DIR
    rm swgui.7z
}

BASE_PLATFORM=arm64
run x86_64 macos -os macos-11 $* # goes first because we might update abi and will not able to build next version until deps are uploaded
run arm64 macos -os macos-12.0 $*
