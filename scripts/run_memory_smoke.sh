#!/usr/bin/env bash
set -euo pipefail
rm -rf data && mkdir -p data
./build/home_assistant --mem-set name "Vincent"
./build/home_assistant --mem-set ville "Paris"
./build/home_assistant --mem-get name
./build/home_assistant --mem-list
./build/home_assistant --note-add "Acheter du lait"
./build/home_assistant --note-list
./build/home_assistant --rem-add "Sortir le chien" --rem-when "2025-08-16T08:00:00"
./build/home_assistant --rem-list
echo "[ok] memory smoke"
