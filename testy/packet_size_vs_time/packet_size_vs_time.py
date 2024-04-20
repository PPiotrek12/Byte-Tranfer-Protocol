import os
import time
import pandas as pd

START = 500
END = 64000
STEP = 500
REPS = 1

counter = 0
for size in range(START, END, STEP):
    for i in range(REPS):
        counter += 1
print(f"Total iterations: {counter}")

results = pd.DataFrame(columns=["size", "time"])
all_times = []
all_sizes = []
print("size,time")
for size in range(START, END, STEP):
    times_list = []
    for i in range(REPS):
        begin = time.time()
        os.system(f"../.././ppcbc tcp 192.168.42.10 10002 {size} < ../../input10MB")
        end = time.time()
        times_list.append(end - begin)
    all_times.append(sum(times_list) / len(times_list))
    all_sizes.append(size)
    print(f"{size},{all_times[-1]}")

results["size"] = all_sizes
results["time"] = all_times
results.to_csv("packet_size_vs_time.csv", index=False)