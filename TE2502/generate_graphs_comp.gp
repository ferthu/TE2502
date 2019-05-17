reset

#set grid


tc = 3

terrains = "elev mount rain"

array colors[tc]
colors[1] = "#396AB1"
colors[2] = "#DA7C30"
colors[3] = "#3E9651"

array raster_titles[tc]
titles[1] = "e = 0.05"
titles[2] = "e = 0.2"
titles[3] = "e = 1.0"

linesize = 2

do for [terrain in terrains]{

	array rasters[tc]
	rasters[1] = terrain."-comp-0.05"
	rasters[2] = terrain."-comp-0.2"
	rasters[3] = terrain."-comp-1.0"

	reset
	set ylabel "similarity index" font ",20"
	set key font ",16"
	set term wxt
	set yrange [0:]
	#set grid
	set xlabel "runtime (s)" font ",20"
	
	plot for [f=1:tc] 'testresults/'.rasters[f].'/com_res.txt' u 1:2 title titles[f] with lines lw linesize linecolor rgb colors[f]
	
	
	set term png
	set output 'testresults/'.terrain.'-comp.png'
	replot
}
