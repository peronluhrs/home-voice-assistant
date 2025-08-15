#!/usr/bin/env bash
set -euo pipefail
echo "If you have websocat: websocat ws://127.0.0.1:8787/ws/events &"
echo "Then run the conversation loop with http enabled:"
echo "./build/home_assistant --http --loop --loop-max-turns 1 --offline"
