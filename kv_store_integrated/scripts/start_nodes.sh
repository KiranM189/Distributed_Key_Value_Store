#!/bin/bash

# Default build directory
BUILD_DIR="../build"
if [ ! -z "$1" ]; then
    BUILD_DIR="$1"
fi

# Check for server binary
if [ ! -f "$BUILD_DIR/kvm_server" ]; then
    echo "Error: kvm_server executable not found in $BUILD_DIR"
    echo "Usage: $0 [path_to_build_directory]"
    exit 1
fi

# Set your IP address (per host)
#MY_IP=$(hostname -I | awk '{print $1}')

# Export FI_PROVIDER to avoid ambiguity
export FI_PROVIDER=tcp

# Get the hostname
HOSTNAME=$(hostname)
HOST_IP=$(hostname -I | awk '{print $1}')

# Start the KV server on the current machine
echo "Starting KV server on $HOSTNAME ($HOST_IP)"
echo "This node will handle $(if [[ "$HOST_IP" == "10.10.3.49" ]]; then echo "ODD"; else echo "EVEN"; fi) keys"

./kvm_server ofi+tcp 8080 &
SERVER_PID=$!

echo "Server started with PID: $SERVER_PID"
echo "Server endpoint: ofi+tcp://$HOST_IP:8080"
echo "To stop, run: kill $SERVER_PID"
