#!/bin/bash
# WARNING: Although the script allows multiple nodes, 1 pinger and 1 ponger only is recommended.

source common.sh

LOG_FILE="latency_log_$(date +%Y%m%d_%H%M%S).txt"

mapfile -t nodes < nodes_dds_latency.txt

if [ ${#nodes[@]} -lt 2 ]; then
    echo "[!] Error: 2 or more nodes are needed on nodes_dds.txt".
    exit 1
fi 

PUB_NODE=${nodes[0]}
SUB_NODES=("${nodes[@]:1}")

echo "[+] Starting subscribers (pong)..."
for node in "${SUB_NODES[@]}"; do
    sshpass -p "$PASS" ssh -f $SSH_OPTS -p "$PORT" "$USER@$node" \
        "cd $WORKSPACE_DIR/Latency_Test && nohup ./latency_host_runner.sh subscriber > /dev/null 2>&1 &"
done

sleep 3

echo "[+] Saving local registry in: $LOG_FILE"
echo "[+] Starting publisher (ping) in $PUB_NODE and waiting results..."

sshpass -p "$PASS" ssh $SSH_OPTS -p "$PORT" "$USER@$PUB_NODE" \
    "cd $WORKSPACE_DIR/Latency_Test && ./latency_host_runner.sh publisher" | tee "$LOG_FILE" | tee "$LOG_FILE"

echo "[+] Publisher ended. Cleaning subscribers..."
for node in "${SUB_NODES[@]}"; do
    sshpass -p "$PASS" ssh $SSH_OPTS -p "$PORT" "$USER@$node" "pkill -f Latency_Test_bin"
done 

echo "[+] Succesfully ended latency test."