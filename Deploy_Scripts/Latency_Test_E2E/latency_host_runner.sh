#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
$DIR/ld-linux-x86-64.so.2 --library-path $DIR $DIR/Latency_Test_E2E_bin  "$@"
