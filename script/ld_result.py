#!/usr/bin/env python

import subprocess
import time
import matplotlib.pyplot as plt
import pandas as pd
import csv
import sys

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


def main():
    sizes = [2**x for x in range(3, 8)]
    writer = csv.writer(sys.stdout, delimiter=",")

    writer.writerow(["size", "time"])
    for i in range(10):
        for size in sizes:
            exec_time = run_command(size)
            #exec_time = size;
            writer.writerow([size, exec_time])


if __name__ == "__main__":
    main()
