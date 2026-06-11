#!/bin/bash
source ./common.sh

set -o errexit
exec 3< nodes_dds.txt

install_tools() {
    local node=$1

    echo "[+] Installing tools on $node..."

    echo "  [-] Creating workspace directories..."
    sshpass -p $PASS ssh $SSH_OPTS -p $PORT $USER@$node "mkdir -p $WORKSPACE_DIR"

    echo "  [-] Copying test directories..."
    sshpass -p $PASS scp -r -P $PORT ./Latency_Test $USER@$node:$WORKSPACE_DIR/
    sshpass -p $PASS scp -r -P $PORT ./Throughput_Test $USER@$node:$WORKSPACE_DIR/
    
    echo "  [-] Fixing execution permissions on remote node..."
    sshpass -p $PASS ssh $SSH_OPTS -p $PORT $USER@$node "chmod +x $WORKSPACE_DIR/Latency_Test/"
    sshpass -p $PASS ssh $SSH_OPTS -p $PORT $USER@$node "chmod +x $WORKSPACE_DIR/Throughput_Test/"
}

while IFS= read -r node <&3; do
    install_tools "$node"
done < nodes_dds.txt

exec 3<&-