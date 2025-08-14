# Home Voice Assistant

A lightweight, extensible voice assistant for home automation.

## Quick Build

Here are two common build configurations.

### 1. Text-Only Mode (Offline)

This mode is useful for testing the core logic without any audio dependencies.

```bash
# Configure the build (no special options needed)
cmake -S . -B build

# Build the executable
cmake --build build

# Run in offline text mode
./build/home_assistant --offline
```

### 2. Offline Audio Mode (PortAudio without ASR/TTS)

This mode enables audio capture and playback using PortAudio, but without speech recognition or synthesis. It's useful for testing audio device setup.

```bash
# Configure the build, enabling audio
cmake -S . -B build -DWITH_AUDIO=ON

# Build the executable
cmake --build build

# Run in offline audio mode
# This will record 5 seconds of audio and play it back.
./build/home_assistant --with-audio --offline
```
