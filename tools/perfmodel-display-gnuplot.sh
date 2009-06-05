#!/bin/bash

#
# StarPU
# Copyright (C) INRIA 2008-2009 (see AUTHORS file)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU Lesser General Public License in COPYING.LGPL for more details.
#


function compute_mean_and_stddev ()
{

filenamein=$1
arch=$2
filenameout=$3

R --no-save > /dev/null << EOF

table <- read.table("$filenamein")

sizelist <- unique(table[,2])

mins <- c()
maxs <- c()
means <- c()
medians <- c()
sds <- c()
firstquarts <- c()
thirdquarts <- c()

for (size in sizelist)
{
	sublist <- table[table[,2]==size,3]

	meanval <- mean(sublist)
	medianval <- median(sublist)
	sdval <- sd(sublist)
	firstquart <- quantile(sublist, 0.25)
	thirdquart <- quantile(sublist, 0.75)
	#minval <- min(sublist)
	minval <- quantile(sublist, 0.01)
	#maxval <- max(sublist)
	maxval <- quantile(sublist, 0.99)

	means <- c(means, meanval)
	medians <- c(medians, medianval)
	sds <- c(sds, sdval)
	firstquarts <- c(firstquarts, firstquart)
	thirdquarts <- c(thirdquarts, thirdquart)
	mins <- c(mins, minval)
	maxs <- c(maxs, maxval)
}

newtable <- data.frame(unique(sizelist), mins, firstquarts, medians, thirdquarts, maxs, means, sds);
write.table(newtable, file="$filenameout");

EOF

}

PERFMODELDISPLAY=./perfmodel-display

function gnuplot_symbol()
{
symbol=$1

echo "Display symbol $symbol"

# TODO check return value $? of perfmodel-display to ensure we have valid data
cuda_a=`$PERFMODELDISPLAY -s $symbol -a cuda -p a`
cuda_b=`$PERFMODELDISPLAY -s $symbol -a cuda -p b`
cuda_c=`$PERFMODELDISPLAY -s $symbol -a cuda -p c`

cuda_alpha=`$PERFMODELDISPLAY -s $symbol -a cuda -p alpha`
cuda_beta=`$PERFMODELDISPLAY -s $symbol -a cuda -p beta`

cuda_debug=`$PERFMODELDISPLAY -s $symbol -p path-file-debug -a cuda`

echo "CUDA : y = $cuda_a * size ^ $cuda_b + $cuda_c"
echo "CUDA : y = $cuda_alpha * size ^ $cuda_beta"
echo "CUDA : debug file $cuda_debug"

core_a=`$PERFMODELDISPLAY -s $symbol -a core -p a`
core_b=`$PERFMODELDISPLAY -s $symbol -a core -p b`
core_c=`$PERFMODELDISPLAY -s $symbol -a core -p c`

core_alpha=`$PERFMODELDISPLAY -s $symbol -a core -p alpha`
core_beta=`$PERFMODELDISPLAY -s $symbol -a core -p beta`

core_debug=`$PERFMODELDISPLAY -s $symbol -p path-file-debug -a core`

echo "CORE : y = $core_a * size ^ $core_b + $core_c"
echo "CORE : y = $core_alpha * size ^ $core_beta"
echo "CORE : debug file $core_debug"

# get the list of the different sizes of the tasks
cuda_size_list=`cut -f2 $cuda_debug| sort -n |uniq|xargs` 
core_size_list=`cut -f2 $core_debug| sort -n |uniq|xargs` 

core_debug_data="model_$symbol.core.data"
cuda_debug_data="model_$symbol.cuda.data"

# In case we want stddev instead of a cloud of points ...
compute_mean_and_stddev "$core_debug" "core"  "$core_debug_data"
compute_mean_and_stddev "$cuda_debug" "cuda"  "$cuda_debug_data"

# use .. 
# 	"$core_debug_data" usi 2:3:4 with errorbars title "core measured" 

gnuplot > /dev/null << EOF
set term postscript eps enhanced color
set output "model_$symbol.eps"

set xlabel "Size (bytes)"
set ylabel "Execution time (us)"


set size 0.60

set multiplot
set style line 1 lt 1 lw 3 linecolor -1 
set style line 2 lt 2 lw 3 linecolor -1
set logscale x
set logscale y


#set grid y
#set grid x

set format x "10^{%L}"
set format y "10^{%L}"

set key top left 

set key title "Non-linear regression\n(y = {/Symbol a} x@^{{/Symbol b}} + {/Symbol g})"

set bars 4.0

plot $core_a * ( x ** $core_b ) + $core_c ls 1 title "CPU" ,\
	$cuda_a * ( x ** $cuda_b ) + $cuda_c ls 2 title "GPU" ,\
	"$core_debug_data" usi 2:4:3:7:6 with candlesticks  ls 1 notitle whiskerbars ,\
	"$core_debug_data" usi 2:5:5:5:5 with candlesticks  ls 1 notitle ,\
	"$cuda_debug_data" usi 2:4:3:7:6 with candlesticks  ls 2  notitle whiskerbars ,\
	"$cuda_debug_data" usi 2:5:5:5:5 with candlesticks  ls 2  notitle

set noborder
set origin 0.42,0.10
set nologscale xy
set size .15,.25
set tics scale 0 
set xrange [-0.1:0.1]
set noxtics
set noytics
set format y2 "%g %%"
set y2tics (1,25,50,75,99)
set y2tics nomirror
set title "Percentiles"

set xlabel ""
set ylabel ""

set key

plot  "$cuda_debug_data" usi (0):(25):(1):(99):(75) with candlesticks ls 1 notitle whiskerbars ,\
	"$cuda_debug_data" usi (0):(50):(50):(50):(50) with candlesticks ls 1 notitle
unset multiplot

EOF
}

for symbol in $@
do
	gnuplot_symbol $symbol
done
