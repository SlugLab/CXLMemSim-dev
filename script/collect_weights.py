import os
workloads = ["mlc","ld","st","nt-ld","nt-st","ptr-chasing"]

def batch_run():
    os.system("../cmake-build-debug/CXLMemSim")