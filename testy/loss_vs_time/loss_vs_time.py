import os
import time
import pandas as pd
import numpy as np

START = 30
END = 100
STEP = 5
REPS = 1

counter = 0
for loss in np.arange(START, END, STEP):
    for i in range(REPS):
        counter += 1
print(f"Total iterations: {counter}")

results = pd.DataFrame(columns=["loss", "time", "packets"])
all_times = []
all_loses = []
all_packets = []
print("loss,time,packets")
for loss in np.arange(START, END, STEP):
    times_list = []
    packets_list = []
    for i in range(REPS):
        begin = time.time()
        
        with open('/sys/class/net/eth1/statistics/tx_packets', 'r') as f:
            tx_packets_beg = int(f.read())
        with open('/sys/class/net/eth1/statistics/rx_packets', 'r') as f:
            rx_packets_beg = int(f.read())


        os.system(f"sudo tc qdisc change dev eth1 root netem loss {loss}%")
        #os.system(f"sudo tc qdisc change dev eth1 root netem delay 10ms")
        os.system(f"../.././ppcbc tcp 192.168.42.10 10001 {64000} < ../../input10MB")


        with open('/sys/class/net/eth1/statistics/tx_packets', 'r') as f:
            tx_packets_end = int(f.read())
        with open('/sys/class/net/eth1/statistics/rx_packets', 'r') as f:
            rx_packets_end = int(f.read())

        end = time.time()
        times_list.append(end - begin)
        packets_list.append(tx_packets_end - tx_packets_beg + rx_packets_end - rx_packets_beg)

    all_times.append(sum(times_list) / len(times_list))
    all_packets.append(sum(packets_list) / len(packets_list))
    all_loses.append(loss)
    print(f"{loss},{all_times[-1]},{all_packets[-1]}")

results["loss"] = all_loses
results["time"] = all_times
results["packets"] = all_packets
results.to_csv("loss_vs_time.csv", index=False)