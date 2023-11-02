import csv
import matplotlib.pyplot as plt

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

