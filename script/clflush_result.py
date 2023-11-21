import subprocess
import time
import matplotlib.pyplot as plt

workloads = ["mlc", "ld", "st", "nt-ld", "nt-st", "ptr-chasing"]


def run_command(size):
    start_time = time.time()
    cmd = ["../cmake-build-debug/microbench/ld" + str(size)]
    print(cmd)
    subprocess.run(cmd)
    end_time = time.time()
    return end_time - start_time


sizes =[2 ** x for x in range(3, 8)]
execution_times = []

#for size in sizes:
#    exec_time = run_command(size)
#    execution_times.append(exec_time)
#    print(f"Size: {size}, Time: {exec_time} seconds")
execution_times = [3.624898672103882,3.2063732147216797,3.0859060287475586,3.0655856132507324,2.7076871395111084]
plt.plot(sizes, execution_times, marker='o')
plt.xlabel('Size')
plt.ylabel('Execution Time (seconds)')
plt.title('Execution Time vs Size')
plt.grid(True)
plt.savefig('ld.png')
