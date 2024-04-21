import os
import time
import pandas as pd
import numpy as np

START = 0
END = 101
STEP = 1
REPS = 1

counter = 0
for delay in np.arange(START, END, STEP):
    for i in range(REPS):
        counter += 1
print(f"Total iterations: {counter}")

results = pd.DataFrame(columns=["delay", "time", "packets"])
all_times = []
all_delays = []
all_packets = []
print("delay,time,packets")
for delay in np.arange(START, END, STEP):
    times_list = []
    packets_list = []
    for i in range(REPS):
        with open('/sys/class/net/eth1/statistics/tx_packets', 'r') as f:
            tx_packets_beg = int(f.read())
        with open('/sys/class/net/eth1/statistics/rx_packets', 'r') as f:
            rx_packets_beg = int(f.read())

        #os.system(f"sudo tc qdisc change dev eth1 root netem loss {loss}%")
        os.system(f"sudo tc qdisc change dev eth1 root netem delay {delay}ms")

        begin = time.time()
        
        os.system(f"../.././ppcbc tcp 192.168.42.10 10001 64000 < ../../input10MB")

        end = time.time()

        with open('/sys/class/net/eth1/statistics/tx_packets', 'r') as f:
            tx_packets_end = int(f.read())
        with open('/sys/class/net/eth1/statistics/rx_packets', 'r') as f:
            rx_packets_end = int(f.read())

        
        times_list.append(end - begin)
        packets_list.append(tx_packets_end - tx_packets_beg + rx_packets_end - rx_packets_beg)

    all_times.append(sum(times_list) / len(times_list))
    all_packets.append(sum(packets_list) / len(packets_list))
    all_delays.append(delay)
    print(f"{delay},{all_times[-1]},{all_packets[-1]}")

results["delay"] = all_loses
results["time"] = all_times
results["packets"] = all_packets
results.to_csv("delay_vs_time.csv", index=False)