#!/bin/bash
# compile one source with the exact MetroidPrime CIOWinCtor.cpp flags
# usage: probe_cc.sh <src> <out.o>
set -uo pipefail
cd "$(dirname "$0")/.."
MP=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
build/tools/wibo build/tools/sjiswrap.exe "$MP/build/compilers/GC/2.7/mwcceppc.exe" \
  -nodefaults -proc gekko -align powerpc -enum int -fp hardware \
  -Cpp_exceptions off -O4,p -inline auto \
  -pragma "cats off" -pragma "warn_notinlined off" \
  -maxerrors 1 -nosyspath -RTTI off -fp_contract on -str reuse \
  -i include -i libc -i build/G2ME01/include \
  -DBUILD_VERSION=0 -DVERSION_G2ME01 -multibyte -DNDEBUG=1 \
  -use_lmw_stmw on -str reuse,pool,readonly -gccinc \
  -inline deferred,noauto -common on -lang=c++ \
  -c "$1" -o "$2"
