#!/usr/bin/env python

import subprocess
import time
import matplotlib.pyplot as plt
import pandas as pd
import csv
import sys, os

workloads = ["mlc", "ld", "st", "nt-ld", "nt-st", "ptr-chasing"]


def run_command(size):
    start_time = time.time()
    cmd = [
        "/usr/bin/numactl -m 1 ../cmake-build-debug/microbench/ld" + str(size),
    ]
    print(cmd)
    process = subprocess.Popen(
        cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )

    out, err = process.communicate()
    print(err)
    return int(out)


def run_cxlmemsim_command(size):
    # start_time = time.time()
    cmd = [
        "LOGV=1",
        "../cmake-build-debug/CXLMemSim",
        "-t",
        "../cmake-build-debug/microbench/ld" + str(size),
        "-i",
        "100",
    ]
    cmd = " ".join(cmd)
    print(cmd)
    os.system(cmd)
    # end_time = time.time()
    df = pd.read_csv("./output_pmu.csv")
    os.system(f"mv ./output_pmu.csv ./ld_pmu{size}_results.csv")
    return df


def main():
    sizes = [2**x for x in range(0, 9)]

    f = open("ld_results_remote.csv", "a")

    writer = csv.writer(f, delimiter=",")

    writer.writerow(["size", "time"])
    for i in range(10):
        for size in sizes:
            exec_time = run_command(size)
            writer.writerow([size, exec_time])

    # for size in sizes:
    #     df = run_cxlmemsim_command(size)


if __name__ == "__main__":
    main()
