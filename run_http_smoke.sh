#!/usr/bin/env bash
set -euo pipefail
pkill -f home_assistant || true
mkdir -p logs
./build/home_assistant --http --http-port 8787 --no-ws --offline &
PID=$!
sleep 0.5
curl -s http://127.0.0.1:8787/api/health
curl -s http://127.0.0.1:8787/api/version
curl -s http://127.0.0.1:8787/api/memory/facts
curl -s -X POST http://127.0.0.1:8787/api/memory/set -H 'Content-Type: application/json' -d '{"key":"name","value":"Vincent"}'
curl -s -X POST http://127.0.0.1:8787/api/memory/get -H 'Content-Type: application/json' -d '{"key":"name"}'
kill $PID
echo "[ok] HTTP smoke"
