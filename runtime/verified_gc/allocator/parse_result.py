import csv
from collections import defaultdict
import matplotlib.pyplot as plt

mp = defaultdict(lambda: [])
with open('./result_with_same_initial_heap_size_v2.csv', newline='') as csvfile:
    reader = csv.reader(csvfile, delimiter=',', quotechar='|')
    all_rows = [line for line in reader]
    for row in all_rows[1:]:
        cmd = row[0]
        name, depth = cmd.split()
        mp[name].append(( int(depth), float( row[1] )) )

keys = sorted(list( mp.keys() ))
datas = []

times= []
depth = []
for key in keys:
    data = sorted( mp[key])[4:]
    X = list(map(lambda x: x[0],data))
    Y = list(map(lambda x: x[1], data))
    if(len( depth )==0):
        depth = X
    times.append(Y)
    plt.plot(X,Y , label=key)

csv_content = []
csv_content.append( "Depth," + ",".join( keys ))
for i in range(len(depth)):
    csv_content.append( ",".join(map(str, [ depth[i], times[0][i], times[1][i], times[2][i]  ]))  )
csv_content = list( map(lambda x : x+ "\n", csv_content) )

open("results_final_output.csv", "w").writelines(csv_content)

plt.xlabel("Binary Tree Depth")
plt.ylabel("Time Taken")
plt.legend()

plt.show()
