#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
mkdir -p build/windows
zig_tool=${ZIG:-zig}
xxd -i < matlab/arnold_sample.png > build/windows/sample.inc

"$zig_tool" cc -target x86_64-windows-gnu -Os -Wall -Wextra \
  -c windows/arnold.c -o build/windows/arnold.obj
"$zig_tool" c++ -target x86_64-windows-gnu -std=c++17 -Os -s -municode \
  -Wl,--subsystem,windows -Wall -Wextra -I build/windows \
  windows/PixelLoop.cpp build/windows/arnold.obj \
  -luser32 -lgdi32 -lgdiplus -lcomdlg32 -lole32 -o build/windows/PixelLoop.exe
