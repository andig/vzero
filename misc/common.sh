#!/bin/bash

function build_sketch()
{
    local srcpath=$1
    local platform=$2
    local sketches=$(find $srcpath -name *.ino)
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        if [[ -f "$sketchdir/.$platform.skip" ]]; then
            echo -e "\n\n ------------ Skipping $sketch ------------ \n\n";
            continue
        fi
        echo -e "\n\n ------------ Building $sketch ------------ \n\n";
        arduino --verify $sketch;
        local result=$?
        if [ $result -ne 0 ]; then
            echo "Build failed ($1)"
            return $result
        fi
    done
}