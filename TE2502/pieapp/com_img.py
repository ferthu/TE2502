import subprocess
import os
from os.path import isfile, join
from os import listdir
from subprocess import check_output
from subprocess import Popen, PIPE
import datetime

subdirs = next(os.walk('./../testresults'))[1]
count = 0
for dir in subdirs:
	path = "../testresults/"+dir
	if not os.path.isfile(path+"/com_res.txt"):
		files = [f for f in listdir(path+"/ray/") if isfile(join(path+"/ray/", f))]
		#f=open(path+"/com_res.txt","w+")
		time=0.25
		for file in files:
			f=open(path+"/com_res.txt","a+")
			print(file)
			print(datetime.datetime.now())
			ref_path = path + "/ray/" + file
			a_path = path + "/rast/" + file
			p=Popen(".\PieAPPv0.1.exe --ref_path "+ref_path+" --A_path "+a_path+" --sampling_mode dense", shell=True, stdout=PIPE)
			#(output, err) = p.communicate()
			#p_status = p.wait()
			#words = output.split()
			words = p.communicate()[0].decode("utf-8").split()
			f.write("{}\t{}\r\n".format(time, words[-1]))
			time+=0.25
			f.close()
		count += 1
print(count)
print(datetime.datetime.now())