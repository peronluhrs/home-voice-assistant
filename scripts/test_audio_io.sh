#!/bin/bash

# Script to test audio input/output devices for the home_assistant application.

# Path to the executable
EXECUTABLE="./build/home_assistant"

# --- Check for executable ---
if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable not found at $EXECUTABLE"
    echo "Please build the project first using CMake."
    echo "Example: cmake -S . -B build -DWITH_AUDIO=ON && cmake --build build"
    exit 1
fi

# --- Step 1: List Devices ---
echo "--- Available Audio Devices ---"
"$EXECUTABLE" --list-devices
echo "-------------------------------"
echo

# --- Step 2: Prompt for User Input ---
echo "Please enter the index or a part of the name of the OUTPUT device you want to test."
read -p "Device: "  output_device

# --- Step 3: Validate Input ---
if [ -z "$output_device" ]; then
    echo "No device entered. Exiting."
    exit 1
fi

# --- Step 4: Run Test ---
echo
echo "Attempting to play a 1-second test tone on '$output_device'..."
"$EXECUTABLE" --output-device "$output_device"

echo
echo "Test complete. If you did not hear a sound, please check your system volume"
echo "and ensure the correct device was selected."
