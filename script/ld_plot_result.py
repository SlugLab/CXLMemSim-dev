#!/usr/bin/env python

import argparse
import subprocess
import time
from math import sqrt
import matplotlib.pyplot as plt
import pandas as pd

workloads = ["mlc", "ld", "st", "nt-ld", "nt-st", "ptr-chasing"]

def get_mean_and_ebars(df, groups, select):
    '''returns df with error bars. gropus includes columns to groupby'''
    agg = df.groupby(groups)[select].agg(['mean', 'count', 'std'])
    error = []
    for i in agg.index:
        mean, count, std = agg.loc[i]
        error.append(1.95 * std/sqrt(count))

    agg['error'] = error

    return agg[['mean']], agg[['error']]


def main():
    parser = argparse.ArgumentParser(description="plot results.")
    parser.add_argument(
        "-f", "--file_name", nargs="?", default="ld_results.csv",
        help="csv containing results.")

    args = parser.parse_args()

    df = pd.read_csv(args.file_name)
    means, error = get_mean_and_ebars(df, ["size"], "time")


    ax = means.plot(yerr=error, grid=True)
    ax.set_xlabel("Size")
    ax.set_ylabel("Execution Time (seconds)")


    fig = ax.get_figure()
    fig.savefig("ld_results.png")

#    plt.plot(sizes, execution_times, marker="o")
#    plt.xlabel("Size")
#    plt.ylabel("Execution Time (seconds)")
#    plt.title("Execution Time vs Size")
#    plt.grid(True)
#    plt.savefig("ld.png")

if __name__ == "__main__":
    main()
