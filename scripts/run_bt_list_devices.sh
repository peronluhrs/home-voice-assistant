#!/bin/bash
set -euo pipefail
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_AUDIO=ON
cmake --build .
echo "--- Listing audio devices ---"
./home_assistant --list-devices
echo "-----------------------------"
echo "Example usage with a Bluetooth device:"
echo "./home_assistant --with-audio --output-device \"bluez\" --record-seconds 3"
