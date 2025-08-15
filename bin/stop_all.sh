#!/usr/bin/env bash
fuser -k 5000/tcp 2>/dev/null || true
pkill -f cloudflared 2>/dev/null || true
echo "OK : services arrêtés."
