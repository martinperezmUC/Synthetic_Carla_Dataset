#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
$DIR/ld-linux-x86-64.so.2 --library-path $DIR $DIR/Throughput_Test_bin "$@"
