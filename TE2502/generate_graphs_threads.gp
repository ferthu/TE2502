reset

tc = 9

terrains = "elev rain mount"

array colors[tc]
colors[1] = "#396AB1"
colors[2] = "#DA7C30"
colors[3] = "#3E9651"
colors[4] = "#6B4C9A"
colors[5] = "#922428"
colors[6] = "#948B3D"

array titles[tc]
titles[1] = "threads = 1"
titles[2] = "threads = 2"
titles[3] = "threads = 4"
titles[4] = "threads = 6"
titles[5] = "threads = 8"
titles[6] = "threads = 10"
titles[7] = "threads = 12"
titles[8] = "threads = 14"
titles[9] = "threads = 16"

array rasters[tc]

linesize = 2

set term png
set tics font ",16"
set ytics offset 1.0,-0.2
set key font ",18"
set xlabel "runtime (s)" font ",20"

set key left top

set ylabel "algorithm iterations / second" font ",20"

rasters[1] = "mount-threads-1"
rasters[2] = "mount-threads-2"
rasters[3] = "mount-threads-4"
rasters[4] = "mount-threads-6"
rasters[5] = "mount-threads-8"
rasters[6] = "mount-threads-10"
rasters[7] = "mount-threads-12"
rasters[8] = "mount-threads-14"
rasters[9] = "mount-threads-16"

set output 'testresults/mount-threads.png'
#set yrange [0:350]
#set ytics 0,70,350

plot for [f=1:6] 'testresults/'.rasters[f].'-final/alg_its.txt' u 1:5 title titles[f] with lines lw linesize linecolor rgb colors[f]



