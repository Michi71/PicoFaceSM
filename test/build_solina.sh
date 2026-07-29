#!/usr/bin/env bash
# build_solina.sh -- macOS host build of the Solina engine
# Compiles the *unmodified* Pico engine (src/solina/) together with
# test/solina_test.cpp against CoreAudio + PortMidi. The Pico audio subsystem
# is switched out through -DSOLINA_HOST_BUILD (see the guard in
# include/solina/solina_defs.h).
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/test/solina_test"

CXX="${CXX:-c++}"
CXXFLAGS=(-std=c++17 -O2 -Wall -DSOLINA_HOST_BUILD
          -I"$ROOT/include"
          -I/opt/homebrew/include)
LDFLAGS=(-L/opt/homebrew/lib -lportmidi
         -framework CoreAudio -framework AudioToolbox
         -framework CoreFoundation)

SRC=("$ROOT/test/solina_test.cpp"
     "$ROOT/src/solina/solina.cpp"
     "$ROOT/src/solina/solina_keyboard.cpp"
     "$ROOT/src/solina/solina_registers.cpp"
     "$ROOT/src/solina/solina_ensemble.cpp"
     "$ROOT/src/solina/solina_phaser.cpp")

echo "[build] $CXX ${CXXFLAGS[*]} ..."
"$CXX" "${CXXFLAGS[@]}" "${SRC[@]}" -o "$OUT" "${LDFLAGS[@]}"

echo "[ok]   $OUT"
