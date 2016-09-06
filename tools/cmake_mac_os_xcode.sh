#!/bin/bash

cmake -H. -Bbuild_xcode \
    -DBISON_EXECUTABLE=/usr/local/Cellar/bison/3.0.4/bin/bison \
    -G Xcode \
    $*

