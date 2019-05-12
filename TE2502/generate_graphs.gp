reset

#set title "CHANGE THIS" font ",20"
set xlabel "Runtime (s)" font ",16"
#set ylabel "Count"
set grid

count = 4
array h[count] # Headings
h[1] = "Generated Nodes"
h[2] = "Drawn Nodes"
h[3] = "Generated Triangles"
h[4] = "Drawn Triangles"

array h2[count] # Short headings
h2[1] = "gen-nodes"
h2[2] = "drawn-nodes"
h2[3] = "gen-tris"
h2[4] = "drawn-tris"

folders = "test1 test2 test3"

linesize = 3

do for [i=1:count] {
	set term wxt
	set ylabel h[i] font ",16"
	set yrange [0:]
	
	plot for [f in folders] 'testresults/'.f.'/draw.txt' u 1:i+1 with lines title f lw linesize
	
	
	set term png
	set output 'testresults/'.h2[i].'.png'
	replot
}

set term wxt
set ylabel "Framerate (Hz)" font ",16"
set yrange [0:]
plot for [f in folders] 'testresults/'.f.'/fps.txt' u 1:2 with lines title f lw linesize

set term png
set output 'testresults/fps.png'
replot