reset

#set grid

set ylabel "frame rate (Hz)" font ",20" offset 0.3,0
set xlabel "runtime (s)" font ",20"

tc = 3
array terrains[tc]
terrains[1] = "mount"
terrains[2] = "rain"
terrains[3] = "elev"

array titles[tc]
titles[1] = "Elevated"
titles[2] = "Mountains"
titles[3] = "Rainforest"

array colors[tc]
colors[1] = "#6B4C9A"
colors[2] = "#922428"
colors[3] = "#948B3D"

set key font ",18"

set term wxt
set xrange [0:20]
set yrange [0:2500]
set tics font ", 16" 
set ytics offset 1.0,-0.2

#set ytics 

plot for [f=1:tc] 'testresults/'.terrains[f].'-simple/fps.txt' u 1:2 every 5 title titles[f] with lines lw 2 linecolor rgb colors[f]


set term png
set output 'testresults/simple-fps.png'
replot
