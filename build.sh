#!/bin/bash
set -eu
shopt -s globstar extglob
cd "$(dirname "$0")"

mkdir -p build

c_flags="-I../src -O3 -flto -ffast-math -fno-unwind-tables -fno-asynchronous-unwind-tables -DBLAKE3_NO_SSE41"
ld_flags=""

compile_cmd="clang $c_flags $ld_flags"

cd build
sources=(../src/*!(external)/!(*.ex).c)
bear --output ../compile_commands.json -- $compile_cmd "${sources[@]}" -o simplechunking
cd ..