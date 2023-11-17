import csv
import matplotlib.pyplot as plt
import os


# TODO Load from perf
def get_perfmon(path: str) -> dict:
    with open(path, 'r') as f:
        reader = csv.reader(f)
        next(reader)
        x = []
        y = []
        for row in reader:
            x.append(float(row[0]))
            y.append(float(row[1]))
    return x, y


def batch_pmu_run(pmu: dict):
    os.system("")
    pass


def draw_graph(x, y):
    plt.plot(x, y, marker='o')
    plt.xlabel('Distance in meters')
    plt.ylabel('Gravitational force in newtons')
    plt.title('Gravitational force and distance')
    plt.show()


def load_csv(path):
    with open(path, 'r') as f:
        reader = csv.reader(f)
        next(reader)
        x = []
        y = []
        for row in reader:
            x.append(float(row[0]))
            y.append(float(row[1]))
    return x, y
