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
    e = {0: [3.1238465309143066, 3.0381410121917725, 3.368220090866089, 3.4121861457824707, 4.9471142292022705], 1: [4.523139238357544, 2.777743101119995, 3.9633989334106445, 4.434335231781006, 3.5737507343292236], 2: [5.711573839187622, 3.4004416465759277, 3.5140678882598877, 3.204282522201538, 2.748567819595337], 3: [3.764162540435791, 4.154339790344238, 5.574926376342773, 3.4832496643066406, 3.0211448669433594], 4: [3.0592517852783203, 3.339315414428711, 4.561910629272461, 5.52558970451355, 2.7204835414886475], 5: [5.511711120605469, 3.6333930492401123, 3.8764026165008545, 3.283872127532959, 3.443842887878418], 6: [4.551557302474976, 2.752399444580078, 3.176825523376465, 2.728515386581421, 2.709603786468506], 7: [2.74589204788208, 2.7224695682525635, 3.213388681411743, 2.7233643531799316, 4.758423328399658], 8: [4.855575799942017, 2.7700676918029785, 2.998997926712036, 2.7254865169525146, 4.403449058532715], 9: [2.775848388671875, 3.3246004581451416, 2.7352170944213867, 2.7170565128326416, 2.726222515106201]}

    writer = csv.writer(sys.stdout, delimiter=",")

    writer.writerow(["size", "time"])
    for i in range(10):
        for size in range(5):
            #exec_time = size;
            writer.writerow([2 ** (size+3), e[i][size]])

#
# for i in range(10):
#     for size in range(5):
#         writer.writerow([2**(size+3), e[i][size]])
if __name__ == "__main__":
    main()
