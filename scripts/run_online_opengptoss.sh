#!/bin/bash
set -euo pipefail

export API_BASE=${API_BASE:-http://localhost:8000/v1}
export API_KEY=${API_KEY:-EMPTY}

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./home_assistant
