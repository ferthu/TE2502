reset

#set title "CHANGE THIS" font ",20"
#set ylabel "Count"

hc = 4
array h[hc] # Headings
h[1] = "generated nodes / second"
h[2] = "rendered nodes / second"
h[3] = "generated triangles / second"
h[4] = "rendered triangles / second"

array h2[hc] # Short headings
h2[1] = "gen-nodes"
h2[2] = "drawn-nodes"
h2[3] = "gen-tris"
h2[4] = "drawn-tris"

rc = 4
array rasters[rc]

array raster_titles[rc]
raster_titles[1] = "e = 0.05"
raster_titles[2] = "e = 0.2"
raster_titles[3] = "e = 1.0"
raster_titles[4] = "ray marching"

array colors[rc]
colors[1] = "#396AB1"
colors[2] = "#DA7C30"
colors[3] = "#3E9651"
colors[4] = "#922428"

linesize = 2
keysize = 18
fontsize = 20
ticssize = 16

terrains = "elev mount rain"

set term png
set xlabel "runtime (s)" font ", ".fontsize
set key font ", ".keysize
set tics font ", ".ticssize 
set ytics offset 1.0,-0.2

# generated nodes --------------------------------------------------------------------
do for [terrain in terrains]{
	rasters[1] = terrain."-rast-0.05"
	rasters[2] = terrain."-rast-0.2"
	rasters[3] = terrain."-rast-1.0"
	i=1
	set output 'testresults/'.terrain."-".h2[i].'.png'
	set ylabel h[i] font ", ".fontsize offset 0.3,0
	set yrange [0:50]
	
	plot for [f=1:rc-1] 'testresults/'.rasters[f].'-final/draw.txt'u 1:i+1 title raster_titles[f] with lines lw linesize linecolor rgb colors[f]
}


# generated triangles ----------------------------------------------------------------
do for [terrain in terrains]{
	rasters[1] = terrain."-rast-0.05"
	rasters[2] = terrain."-rast-0.2"
	rasters[3] = terrain."-rast-1.0"
	i=3
	set output 'testresults/'.terrain."-".h2[i].'.png'
	set ylabel h[i] font ", ".fontsize offset 0.3,0
	set yrange [0:140000]
	set ytics ("0" 0, "28k" 28000, "56k" 56000, "84k" 84000, "112k" 112000, "140k" 140000)
	
	plot for [f=1:rc-1] 'testresults/'.rasters[f].'-final/draw.txt'u 1:i+1 title raster_titles[f] with lines lw linesize linecolor rgb colors[f]
}


# drawn nodes ---------------------------------------------------------------------
terrains = "elev rain"
do for [terrain in terrains]{
	rasters[1] = terrain."-rast-0.05"
	rasters[2] = terrain."-rast-0.2"
	rasters[3] = terrain."-rast-1.0"
	i=2
	set output 'testresults/'.terrain."-".h2[i].'.png'
	set ylabel h[i] font ", ".fontsize offset 0.3,0
	set yrange [0:50000]
	set ytics ("0" 0, "10k" 10000, "20k" 20000, "30k" 30000, "40k" 40000, "50k" 50000)
	
	plot for [f=1:rc-1] 'testresults/'.rasters[f].'-final/draw.txt'u 1:i+1 title raster_titles[f] with lines lw linesize linecolor rgb colors[f]
}
i=2
terrain = "mount"
rasters[1] = terrain."-rast-0.05"
rasters[2] = terrain."-rast-0.2"
rasters[3] = terrain."-rast-1.0"
set yrange [0:200000]
set ytics ("0" 0, "40k" 40000, "80k" 80000, "120k" 120000, "160k" 160000, "200k" 200000)

set output 'testresults/'.terrain."-".h2[i].'.png'
plot for [f=1:rc-1] 'testresults/'.rasters[f].'-final/draw.txt'u 1:i+1 title raster_titles[f] with lines lw linesize linecolor rgb colors[f]


# drawn triangles ---------------------------------------------------------------------
terrains = "elev rain"
do for [terrain in terrains]{
	rasters[1] = terrain."-rast-0.05"
	rasters[2] = terrain."-rast-0.2"
	rasters[3] = terrain."-rast-1.0"
	i=4
	set output 'testresults/'.terrain."-".h2[i].'.png'
	set ylabel h[i] font ", ".fontsize offset 0.3,0
	set yrange [0:100000000]
	set ytics ("0" 0, "20M" 20000000, "40M" 40000000, "60M" 60000000, "80M" 80000000, "100M" 100000000)
	
	plot for [f=1:rc-1] 'testresults/'.rasters[f].'-final/draw.txt'u 1:i+1 title raster_titles[f] with lines lw linesize linecolor rgb colors[f]
}
i=4
terrain = "mount"
rasters[1] = terrain."-rast-0.05"
rasters[2] = terrain."-rast-0.2"
rasters[3] = terrain."-rast-1.0"
rasters[4] = terrain."-ray"
set yrange [0:800000000]
set ytics ("0" 0, "160M" 160000000, "320M" 320000000, "480M" 480000000, "640M" 640000000, "800M" 800000000)

set output 'testresults/'.terrain."-".h2[i].'.png'
plot for [f=1:rc-1] 'testresults/'.rasters[f].'-final/draw.txt'u 1:i+1 title raster_titles[f] with lines lw linesize linecolor rgb colors[f]


# fps ---------------------------------------------------------------------------------
terrains = "elev rain"
do for [terrain in terrains]{
	rasters[1] = terrain."-rast-0.05"
	rasters[2] = terrain."-rast-0.2"
	rasters[3] = terrain."-rast-1.0"
	rasters[4] = terrain."-ray"
	set output 'testresults/'.terrain.'-fps.png'
	set ylabel "frame rate (Hz)" font ", ".fontsize offset 0.3,0
	set yrange [0:150]
	set ytics 0,30,150
	plot for [f=1:rc] 'testresults/'.rasters[f].'/fps.txt' u 1:2 with lines title raster_titles[f] lw linesize linecolor rgb colors[f]
}
terrain = "mount"
rasters[1] = terrain."-rast-0.05"
rasters[2] = terrain."-rast-0.2"
rasters[3] = terrain."-rast-1.0"
rasters[4] = terrain."-ray"
set output 'testresults/'.terrain.'-fps.png'
set ylabel "frame rate (Hz)" font ", ".fontsize offset 0.3,0
set yrange [0:500]
set ytics 0,100,500
plot for [f=1:rc] 'testresults/'.rasters[f].'/fps.txt' u 1:2 with lines title raster_titles[f] lw linesize linecolor rgb colors[f]





