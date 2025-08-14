#!/bin/bash
set -euo pipefail
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./home_assistant 
