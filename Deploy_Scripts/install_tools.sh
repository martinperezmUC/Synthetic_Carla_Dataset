#!/bin/bash
source common.sh

WORKSPACE_DIR="/home/$USER/performance_tests"

set -o errexit
exec 3< nodes_dds.txt

install_tools() {
    local node=$1

    echo "[+] Installing tools on $USER:$node..."

    echo "\t[-] Creating workspace directories..."
    sshpass -p $PASS ssh -p $PORT $USER@$node "mkdir -p $WORKSPACE_DIR/Latency_Test"
    sshpass -p $PASS ssh -p $PORT $USER@$node "mkdir -p $WORKSPACE_DIR/Throughput_Test"

    echo "\t[-] Copying executables..."
    sshpass -p $PASS scp -P $PORT ./Latency_Test $USER@$node:$WORKSPACE_DIR/Latency_Test/
    sshpass -p $PASS scp -P $PORT ./Throughput_Test $USER@$node:$WORKSPACE_DIR/Throughput_Test/
}

while IFS= read -r node <&3; do
    install_tools "$node"
done < nodes_dds.txt

exec 3<&-