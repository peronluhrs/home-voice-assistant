#!/bin/bash
set -e

# --- Config ---
EXE_PATH="./build/home_assistant"
HTTP_PORT=8787
BASE_URL="http://127.0.0.1:${HTTP_PORT}"
PID_FILE="server.pid"

# --- Helper Functions ---
function start_server() {
    echo "Starting server with flags: $@"
    $EXE_PATH --http --http-port $HTTP_PORT --offline --loop "$@" &
    echo $! > $PID_FILE
    sleep 0.5 # Give it time to start
    if ! ps -p $(cat $PID_FILE) > /dev/null; then
        echo "Server failed to start."
        exit 1
    fi
    echo "Server started with PID $(cat $PID_FILE)"
}

function stop_server() {
    if [ -f $PID_FILE ]; then
        PID=$(cat $PID_FILE)
        echo "Stopping server with PID $PID..."
        kill $PID || true
        rm $PID_FILE
        # Wait a moment to ensure the port is free
        sleep 0.5
    fi
}

function run_test() {
    echo "--- Running test: $1 ---"
    eval $2
    echo "--- Test passed: $1 ---"
    echo
}

# --- Cleanup on exit ---
trap stop_server EXIT

# --- Test Cases ---

# 1. Basic smoke test (no auth)
test_basic_smoke() {
    start_server
    curl -s $BASE_URL/api/health | grep -q '"ok":true'
    curl -s $BASE_URL/api/version | grep -q '"version":"dev"'
    stop_server
}

# 2. Auth check
test_auth() {
    start_server --http-bearer SECRET
    # Expect 401 without token
    test "$(curl -s -o /dev/null -w '%{http_code}' $BASE_URL/api/health)" = "401"
    # Expect 200 with token
    test "$(curl -s -H 'Authorization: Bearer SECRET' -o /dev/null -w '%{http_code}' $BASE_URL/api/health)" = "200"
    stop_server
}

# 3. Memory endpoints
test_memory() {
    start_server
    # Set a value
    curl -s -X POST -H 'Content-Type: application/json' -d '{"facts":{"note":"hello"}}' $BASE_URL/api/memory
    # Get the value
    curl -s -X GET $BASE_URL/api/memory | grep -q '"note":"hello"'
    # Delete the value
    curl -s -X DELETE $BASE_URL/api/memory
    # Check it's gone
    ! (curl -s -X GET $BASE_URL/api/memory | grep -q '"note":"hello"')
    stop_server
}

# 4. 501 for disabled features (assuming built with no Vosk/Piper)
test_501() {
    start_server
    # TTS
    test "$(curl -s -o /dev/null -w '%{http_code}' -X POST -H 'Content-Type: application/json' -d '{"text":"..."}' $BASE_URL/api/tts)" = "501"
    # ASR
    test "$(curl -s -o /dev/null -w '%{http_code}' -X POST --data-binary @- $BASE_URL/api/asr < /dev/zero)" = "501"
    stop_server
}


# --- Run all tests ---
run_test "Basic Smoke Test" test_basic_smoke
run_test "Authentication" test_auth
run_test "Memory Endpoints" test_memory
# The 501 test should only be run on a build without the features.
# For now, we assume this script is run against a minimal build.
echo "NOTE: The following test assumes the binary was built with Vosk/Piper OFF."
run_test "501 Not Implemented" test_501

echo "All smoke tests passed!"
