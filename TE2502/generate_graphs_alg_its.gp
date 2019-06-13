reset

tc = 3

array colors[tc]
colors[1] = "#396AB1"
colors[2] = "#DA7C30"
colors[3] = "#3E9651"

array titles[tc]
titles[1] = "e = 0.05"
titles[2] = "e = 0.2"
titles[3] = "e = 1.0"

array rasters[tc]

linesize = 2

set term png
set tics font ",16"
set ytics offset 1.0,-0.2
set key font ",18"
set xlabel "runtime (s)" font ",20"

# Algorithm iterations ------------------------------------------------------------
set ylabel "algorithm iterations / second" font ",20"
set key top left
terrains = "elev rain mount"
do for [terrain in terrains]{
	rasters[1] = terrain."-rast-0.05"
	rasters[2] = terrain."-rast-0.2"
	rasters[3] = terrain."-rast-1.0"

	set output 'testresults/'.terrain.'-alg-its.png'
	set yrange [0:300]
	set ytics 0,60,300
	
	plot for [f=1:tc] 'testresults/'.rasters[f].'-final/alg_its.txt' u 1:5 title titles[f] with lines lw linesize linecolor rgb colors[f]
}
#terrain = "mount"
#rasters[1] = terrain."-rast-0.05"
#rasters[2] = terrain."-rast-0.2"
#rasters[3] = terrain."-rast-1.0"
#set output 'testresults/'.terrain.'-alg-its.png'
#set yrange [0:0.7]
#set ytics 0,0.14,0.7
#plot for [f=1:tc] 'testresults/'.rasters[f].'-final/alg_its.txt' u 1:5 title titles[f] with lines lw linesize linecolor rgb colors[f]

# Subroutine times ----------------------------------------------------------------
terrain = "elev"
set key top right
set ylabel "algorithm subroutine times (s)" font ",20"
set output 'testresults/'.terrain.'-subroutines.png'
set yrange [0:0.3]
set ytics 0,0.06,0.3
file = 'testresults/'.terrain.'-rast-0.05-final/alg_its.txt'
plot file u 1:4 with filledcurves x1 lw linesize lc rgb colors[1] title "Update Triangulation", "" u 1:3 with filledcurves x1 lw linesize lc rgb colors[2] title "Refine Vertex Selection", "" u 1:2 with filledcurves x1 lw linesize lc rgb colors[3] title "Generate Initial Triangulation" 

terrain = "rain"
file = 'testresults/'.terrain.'-rast-0.05-final/alg_its.txt'
set output 'testresults/'.terrain.'-subroutines.png'
set yrange [0:0.35]
set ytics 0,0.07,0.35
plot file u 1:4 with filledcurves x1 lw linesize lc rgb colors[1] title "Update Triangulation", "" u 1:3 with filledcurves x1 lw linesize lc rgb colors[2] title "Refine Vertex Selection", "" u 1:2 with filledcurves x1 lw linesize lc rgb colors[3] title "Generate Initial Triangulation" 


terrain = "mount"
file = 'testresults/'.terrain.'-rast-0.05-final/alg_its.txt'
set output 'testresults/'.terrain.'-subroutines.png'
set yrange [0:0.7]
set ytics 0,0.14,0.7
plot file u 1:4 with filledcurves x1 lw linesize lc rgb colors[1] title "Update Triangulation", "" u 1:3 with filledcurves x1 lw linesize lc rgb colors[2] title "Refine Vertex Selection", "" u 1:2 with filledcurves x1 lw linesize lc rgb colors[3] title "Generate Initial Triangulation" 




