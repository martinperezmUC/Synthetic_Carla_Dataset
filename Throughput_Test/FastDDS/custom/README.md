## Performance Test: Throughput (Bandwith & Saturation)

This test evaluates the maximum data processing capacity and networtk bandwith saturation by measuring the number of messages per second (msg/s), Mbps and MBps.

### Test Architecture
This benchmark uses a **Many-to-One** architecture, simulating a real-world scenario with multiple vehicles sending telemetry to a single edge node on a road sector.

1. **Data Sink (Subscriber / Edge-Road node):** Passive receiver. It continuously listens for incoming data from any number of publishers. It queries the FastCDR serialization engine to calculate the bytes of each received packet.
2. **Data Sources (Publishers / Vehicles):** Independent data generators. They send data structures without waiting for any ACK.

### Frequency Control
To simulate different loads, publishers can be throttle using the `PUB_FREQ` environment variable. If omitted or set to `0`, the publisher will inject data as fast as the CPU or network allows.

### Usage
```bash
# Terminal 1: Start the Edge Node
./Throughput_Test subscriber

# Terminal 2: Spawn Vehicle A sending data at 60 Hz
PUB_FREQ=60 ./Throughput_Test publisher

# Terminal 3: Spawn Vehicle B sending data at 30 Hz
PUB_FREQ=30 ./Throughput_Test publisher

# Terminal 4: Spawn Vehicle C at MAX speed 
./Throughput_Test publisher
```

*Verification tests executed under Kernel: Linux 6.12.90+deb13.1-amd64*