reset

#set title "CHANGE THIS" font ",20"
#set ylabel "Count"

hc = 4
array h[hc] # Headings
h[1] = "generated nodes / second"
h[2] = "drawn nodes / second"
h[3] = "generated triangles / second"
h[4] = "drawn triangles / second"

array h2[hc] # Short headings
h2[1] = "gen-nodes"
h2[2] = "drawn-nodes"
h2[3] = "gen-tris"
h2[4] = "drawn-tris"

array ym[hc] # max y range
ym[1] = 14000 #
ym[2] = 450000 # 60000
ym[3] = 600000
ym[4] = 0.95e9

terrains = "elev mount rain"

do for [terrain in terrains]{

rc = 4
array rasters[rc]
rasters[1] = terrain."-rast-0.05"
rasters[2] = terrain."-rast-0.2"
rasters[3] = terrain."-rast-1.0"
rasters[4] = terrain."-ray"

array raster_titles[rc]
raster_titles[1] = "t=0.05"
raster_titles[2] = "t=0.2"
raster_titles[3] = "t=1.0"
raster_titles[4] = "ray marching"

array colors[rc]
colors[1] = "#396AB1"
colors[2] = "#DA7C30"
colors[3] = "#3E9651"
colors[4] = "#922428"

linesize = 5

set key font ",16"

do for [i=1:hc] {
	reset
	set term wxt
	set ylabel h[i] font ",20"
	set yrange [0:]
	set grid
	set xlabel "runtime (s)" font ",20"
	
	plot for [f=1:rc-1] 'testresults/'.rasters[f].'/draw.txt' u 1:i+1 title raster_titles[f] with lines lw linesize linecolor rgb colors[f]
	
	
	set term png
	set output 'testresults/'.terrain."-".h2[i].'.png'
	replot
}



set term wxt
set ylabel "frame rate (Hz)" font ",20"
set yrange [0:500]
plot for [f=1:rc] 'testresults/'.rasters[f].'/fps.txt' u 1:2 with lines title raster_titles[f] lw linesize linecolor rgb colors[f]

set term png
set output 'testresults/'.terrain.'-fps.png'
replot
}




