#!/bin/bash
set -o nounset
set -o errexit

cd "$(dirname "$0")"


cp     $BIN_DIR/plugins/tools/kitPlugins/librobots-ev3-interpreter.dylib   $PWD/../data/plugins/tools/kitPlugins/
cp -pr $BIN_DIR/librobots-ev3-kit*.dylib                                   $PWD/../data
cp -pr $BIN_DIR/libqextserialport*.dylib                                   $PWD/../data