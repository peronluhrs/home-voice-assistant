#!/bin/bash
set -euo pipefail

export API_BASE=${API_BASE:-http://127.0.0.1:11434/v1}
export MODEL=${MODEL:-llama3.1}

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./home_assistant --model "$MODEL"
