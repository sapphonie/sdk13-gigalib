#!/bin/bash
vers="10.1.1"
rm -rfv fmt-${vers}*

wget https://github.com/fmtlib/fmt/releases/download/${vers}/fmt-${vers}.zip
unzip fmt-${vers}.zip
cd fmt-${vers}


mkdir build          # Create a direct│No version information found in this file.
cd build
CFLAGS=-m32 CXXFLAGS=-m32 cmake ../ -D FMT_TEST=OFF
make
# cmake ..
