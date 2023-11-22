import subprocess
import time
import matplotlib.pyplot as plt
import pandas as pd
import os,csv

workloads = ["mlc", "ld", "st", "nt-ld", "nt-st", "ptr-chasing"]


def run_command(size, mem_node):
    start_time = time.time()
    cmd = [
        f"/usr/bin/numactl -m {mem_node} ../../MLC/Linux/mlc  --loaded_latency -W"
        + str(size),
    ]
    print(cmd)
    process = subprocess.Popen(
        cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )

    out, err = process.communicate()
    print(f"err: {err}, out: {out}")
    return int(out)


def run_cxlmemsim_command(size, mem_node):
    # start_time = time.time()
    cmd = [
        "LOGV=1",
        f"/usr/bin/numactl -m {mem_node}",
        "../cmake-build-debug/CXLMemSim",
        "-t",
        "'../../MLC/Linux/mlc  --loaded_latency -W'"
        "-i",
        "100",
    ]
    cmd = " ".join(cmd)
    print(cmd)
    os.system(cmd)
    # end_time = time.time()
    df = pd.read_csv("./output_pmu.csv")
    os.system(f"mv ./output_pmu.csv ./wb_pmu{size}_results.csv")
    return df



def main():
    sizes = [x for x in range(2, 12)]

    mode = "remote"
    mem_node = 0 if mode == "local" else 1

        
    f = open(f"st_results_{mode}.csv", "a")

    writer = csv.writer(f, delimiter=",")

    writer.writerow(["size", "time"])
    for i in range(25):
        for size in sizes:
            exec_time = run_command(size, mem_node)
            writer.writerow([size, exec_time])

    # for size in sizes:
    #     df = run_cxlmemsim_command(size,1)


if __name__ == "__main__":
    main()
