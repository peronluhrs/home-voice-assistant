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

### 3. Piper TTS Mode

This mode enables text-to-speech synthesis using the Piper TTS engine. Piper is used as an external command-line tool.

**Prerequisites:**
- You must have the `piper` executable in your `PATH`.
- You need a Piper voice model (`.onnx` file).

**Build:**
```bash
# Configure the build, enabling Piper and Audio
cmake -S . -B build -DWITH_PIPER=ON -DWITH_AUDIO=ON

# Build the executable
cmake --build build
```

**Usage:**
```bash
# Synthesize text and speak it
./build/home_assistant \
  --piper-model /path/to/your/voice.onnx \
  --say "Hello, world."

# If piper is not in your PATH, specify its location
./build/home_assistant \
  --piper-bin /path/to/piper \
  --piper-model /path/to/your/voice.onnx \
  --say "Hello, world."

# If you run without --with-audio, the output is saved to captures/tts_say.wav
./build/home_assistant \
  --piper-model /path/to/your/voice.onnx \
  --say "Hello, world."
```
