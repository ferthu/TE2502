import os
from os.path import isfile, join
from os import listdir
import datetime

file_names = ["alg_its","fps","draw"]


# Find all test groups (multiple iterations of the same test setting)
cwd = os.getcwd() + "/testresults/"
testdirs = next(os.walk('./testresults'))[1]
testgroups = {}
for testdir in testdirs:
	if "final" not in testdir:
		dir = testdir.split("_")[0]
		if dir in testgroups:
			testgroups[dir] += 1
		else:
			testgroups[dir] = 1

# Calculate the average values
for file_name in file_names:
	print(file_name)
	for x, count in testgroups.items():
		print(x)
		if not os.path.exists(cwd+x+"-final"):
			os.mkdir(cwd+x+"-final")
		res_file=open(cwd+x+"-final/"+file_name+".txt","w+")
		delta = 0.25
		last_time = 0
		time = delta
		while True:
			data = {}
			amount = 0
			for i in range(1,count+1):
				with open(cwd + x + "_" + str(i) + "/"+file_name+".txt") as f:
					for line in f:
						items = line.split()
						if float(items[0]) > last_time and float(items[0]) < time:
							for d in range(len(items) - 1):
								if not str(d) in data:
									data[str(d)] = 0
								data[str(d)] += float(items[d+1])
							amount += 1
			
			res_file.write(str(time) + "   \t")
			
			if amount == 0:
				res_file.write("0.0   \t")
			else:
				for k,d in data.items():
					d /= amount
					#print(time, d)
					res_file.write(str(d) + "   \t")
			
			res_file.write("\n")
			last_time = time
			time += delta
			if amount == 0 and time > 17:
				break
			
		res_file.close()
