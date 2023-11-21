import subprocess
import time
import matplotlib.pyplot as plt
import pandas as pd

workloads = ["mlc", "ld", "st", "nt-ld", "nt-st", "ptr-chasing"]


def run_command(size):
    start_time = time.time()
    cmd = ["../cmake-build-debug/microbench/ld" + str(size)]
    print(cmd)
    subprocess.run(cmd)
    end_time = time.time()
    return end_time - start_time


def run_cxlmemsim_command(size):
    # start_time = time.time()
    cmd = [
        "../cmake-build-debug/CXLMemSim",
        ' -t "../cmake-build-debug/microbench/ld' + str(size) + '"',
    ]
    print(cmd)
    subprocess.run(cmd)
    # end_time = time.time()
    df = pd.read_csv("../cmake-build-debug/output_pmu.csv")
    return df


sizes = [2**x for x in range(3, 8)]
execution_times = []
plot_datas = []

for i in range(10):
    for size in sizes:
        exec_time = run_command(size)
        execution_times.append(exec_time)
#    print(f"Size: {size}, Time: {exec_time} seconds")

plt.plot(sizes, execution_times, marker="o")
plt.xlabel("Size")
plt.ylabel("Execution Time (seconds)")
plt.title("Execution Time vs Size")
plt.grid(True)
plt.savefig("ld.png")
