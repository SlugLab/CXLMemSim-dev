import matplotlib.pyplot as plt

# Data
x = [1, 10, 100, 1000, 10000]
y = [5914, 2435, 2345, 2298, 2195]

# Create the plot
plt.figure(figsize=(10,5))
plt.plot(x, y, marker='o')


plt.xlabel('PEBS period')
plt.ylabel('Time(s)')
plt.tight_layout()
plt.savefig('pebs_graph.pdf')