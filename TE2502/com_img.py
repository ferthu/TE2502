import subprocess
import os
from os.path import isfile, join
from os import listdir

subdirs = next(os.walk('./testresults'))[1]
count = 0
for dir in subdirs:
	path = "testresults/"+dir
	if not os.path.isfile(path+"/com_res.txt"):
		files = [f for f in listdir(path+"/ray/") if isfile(join(path+"/ray/", f))]
		f=open(path+"/com_res.txt","w+")
		for file in files:
			print(file)
			ref_path = path + "/ray/" + file
			a_path = path + "/rast/" + file
			p=subprocess.popen("pieapp\pieappv0.1.exe --ref_path "+ref_path+" --a_path "+a_path+" --sampling_mode sparse", shell=true, stdout=subprocess.pipe)
			(output, err) = p.communicate()
			p_status = p.wait()
			words = output.split()
			f.write("%s\r\n" % words[-1].decode("utf-8"))
		f.close()
		count += 1
print(count)