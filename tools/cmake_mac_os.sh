#!/bin/bash

cmake -H. -Bbuild \
    -DBISON_EXECUTABLE=/usr/local/Cellar/bison/3.0.4_1/bin/bison \
    $*

