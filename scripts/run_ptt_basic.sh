#!/bin/bash

# Basic Push-to-Talk (PTT) test script.
# Press Enter to start recording, and Enter again to stop.
# The captured audio will be saved in the 'captures' directory.

# Path to the executable
EXE_PATH="./build/home_assistant"

# Check if the executable exists
if [ ! -f "$EXE_PATH" ]; then
    echo "Error: Executable not found at $EXE_PATH"
    echo "Please build the project first (e.g., using cmake . && make)"
    exit 1
fi

# Directory to save WAV files
CAPTURE_DIR="captures"

# Run the command
echo "Running PTT capture. Audio will be saved to '$CAPTURE_DIR/'"
"$EXE_PATH" --ptt --save-wav "$CAPTURE_DIR/" --record-seconds 10

echo "Script finished."
