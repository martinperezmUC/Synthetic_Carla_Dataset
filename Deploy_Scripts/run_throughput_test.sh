#!/bin/bash
source common.sh

LOG_FILE="throughput_log_$(date +%Y%m%d_%H%M%S).txt"

mapfile -t nodes < nodes_dds.txt

if [ ${#nodes[@]} -lt 2 ]; then
    echo "[!] Error: 2 or more nodes are needed on nodes_dds.txt".
    exit 1
fi 

SUB_NODE=${nodes[0]} # First node of the list
PUB_NODES=("${nodes[@]:1}")

echo "[+] Starting Edge Node (Sub) in $SUB_NODE..."
echo "[+] Writing local registry in: $LOG_FILE"

sshpass -p "$PASS" ssh $SSH_OPTS -n -p "$PORT" "$USER@$SUB_NODE" \
    "cd $WORKSPACE_DIR/Throughput_Test && ./throughput_host_runner.sh subscriber" | tee "$LOG_FILE" &

SUB_PID=$!

sleep 3

echo "[+] Starting Vehicles (Pubs) for 60 seconds (MAX SPEED)..."
for node in "${PUB_NODES[@]}"; do
    sshpass -p "$PASS" ssh $SSH_OPTS -f -p "$PORT" "$USER@$node" \
        "cd $WORKSPACE_DIR/Throughput_Test && nohup timeout 60 ./throughput_host_runner.sh publisher > /dev/null 2>&1 &" &
done

echo "[+] Waiting..."

wait $SUB_PID

echo "[+] Succesfully ended throughput test."
