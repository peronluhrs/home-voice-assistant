#!/bin/bash
set -euo pipefail
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_AUDIO=ON
cmake --build .
./home_assistant --with-audio --record-seconds 3
