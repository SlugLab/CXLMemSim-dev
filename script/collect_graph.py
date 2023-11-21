import os
import csv
import pandas as pd
workloads = ["mlc","ld","st","nt-ld","nt-st","ptr-chasing"]


df = pd.read_csv('ld.csv')

with open('ld_new.csv', 'w') as f:

    # Create a CSV writer object that will write to the file 'f'
    csv_writer = csv.writer(f)

    # Write the field names (column headers) to the first row of the CSV file
    csv_writer.writerow(df[1])

    # Write all of the rows of data to the CSV file
    csv_writer.writerows(rows)