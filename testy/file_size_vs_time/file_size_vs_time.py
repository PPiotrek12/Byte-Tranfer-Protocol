import os
import time
import pandas as pd
import numpy as np
import math

START = 100000
END = 1000000
STEP = 1000
REPS = 1

counter = 0
for size in np.arange(START, END, STEP):
    for i in range(REPS):
        counter += 1
print(f"Total iterations: {counter}")

results = pd.DataFrame(columns=["size", "time", "packets"])
all_times = []
all_sizes = []
all_packets = []
print("size,time,packets")
for size in np.arange(START, END, STEP):
    times_list = []
    packets_list = []
    for i in range(REPS):
        with open('/sys/class/net/eth1/statistics/tx_packets', 'r') as f:
            tx_packets_beg = int(f.read())
        with open('/sys/class/net/eth1/statistics/rx_packets', 'r') as f:
            rx_packets_beg = int(f.read())

        os.system(f"../.././file_gen {size} > inp")

        begin = time.time()

        #os.system(f"sudo tc qdisc change dev eth1 root netem loss {loss}%")
        #os.system(f"sudo tc qdisc change dev eth1 root netem delay 10ms")
        
        os.system(f"../.././ppcbc udp 192.168.42.10 10001 1000 < inp")

        end = time.time()

        if end - begin > 1:
            continue

        with open('/sys/class/net/eth1/statistics/tx_packets', 'r') as f:
            tx_packets_end = int(f.read())
        with open('/sys/class/net/eth1/statistics/rx_packets', 'r') as f:
            rx_packets_end = int(f.read())

        times_list.append(end - begin)
        packets_list.append(tx_packets_end - tx_packets_beg + rx_packets_end - rx_packets_beg)

    all_times.append(sum(times_list) / max(1, len(times_list)))
    all_packets.append(sum(packets_list) / max(1, len(packets_list)))
    all_sizes.append(size)
    print(f"{size},{all_times[-1]},{all_packets[-1]}")

results["size"] = all_sizes
results["time"] = all_times
results["packets"] = all_packets
results.to_csv("file_size_vs_time.csv", index=False)