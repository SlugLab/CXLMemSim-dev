import csv
import matplotlib.pyplot as plt
import os
import json


# TODO Load from perf concat file
def get_perfmon(path: str, pmu:list) -> dict:
    data_dict = {}
    cur_csv = json.loads(f.read())
    
    with open(path, 'r') as f:
        for line in pmu:
            
            # Extract the EventName, UMask, and EventCode
            event = cur_csv["Events"][0]
            event_name = event["EventName"]
            umask = event["UMask"]
            event_code = event["EventCode"]

            # Combine UMask and EventCode
            combined_code = umask + event_code[2:]  # Concatenate and remove '0x' from EventCode
            combined_code_hex = "0x" + combined_code[2:]  # Add '0x' back for hex representation

            # Print the results
            print(f"Event Name: {event_name}")
            print(f"Combined UMask and EventCode: {combined_code_hex}")
    return data_dict


def batch_pmu_run(pmu: dict):
    return


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

if __name__ == '__main__':
    pmu = {"INST_RETIRED.ANY" : 0}
    get_perfmon("../cmake-build-debug/output_pmu.csv", pmu)
    # x, y = load_csv('data.csv')
    # draw_graph(x, y)