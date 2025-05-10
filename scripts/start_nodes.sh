#!/bin/bash

# Default build directory and memory settings
BUILD_DIR="../build"
SHARED_MEM_SIZE="500M"  # Allocate 500MB shared memory per node

# Parse command line arguments
while getopts ":d:m:" opt; do
  case $opt in
    d) BUILD_DIR="$OPTARG" ;;
    m) SHARED_MEM_SIZE="$OPTARG" ;;
    \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
    :) echo "Option -$OPTARG requires an argument." >&2; exit 1 ;;
  esac
done

# Check for server binary
if [ ! -f "$BUILD_DIR/kvm_server" ]; then
    echo "Error: kvm_server executable not found in $BUILD_DIR"
    echo "Usage: $0 [-d path_to_build_directory] [-m memory_size]"
    echo "       memory_size can be specified with K, M, or G suffix (e.g., 500M for 500MB)"
    exit 1
fi

# Export FI_PROVIDER to avoid ambiguity
export FI_PROVIDER=tcp

# Get the hostname and IP
HOSTNAME=$(hostname)
HOST_IP=$(hostname -I | awk '{print $1}')

# Echo memory configuration
echo "Memory configuration: $SHARED_MEM_SIZE per node"

# Start the KV server on the current machine
echo "Starting KV server on $HOSTNAME ($HOST_IP)"
echo "This node will handle $(if [[ "$HOST_IP" == "10.10.3.49" ]]; then echo "ODD"; else echo "EVEN"; fi) keys"

# Launch the server with the specified memory size
$BUILD_DIR/kvm_server ofi+tcp 8080 $SHARED_MEM_SIZE &
SERVER_PID=$!

echo "Server started with PID: $SERVER_PID"
echo "Server endpoint: ofi+tcp://$HOST_IP:8080"
echo "Shared memory allocation: $SHARED_MEM_SIZE"
echo "To stop, run: kill $SERVER_PID"
