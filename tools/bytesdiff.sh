#!/bin/bash
# bytesdiff.sh <src.cpp> <symbol-substring> <retail_addr> <size>
set -uo pipefail
cd "$(dirname "$0")/.."
./tools/probe_cc.sh "$1" /tmp/opencode/_b.o || exit 1
python3 tools/bytescmp.py /tmp/opencode/_b.o "$2" "$3" "$4"
