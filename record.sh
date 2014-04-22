#!/usr/bin/env bash

#
# record.sh
#
# Copyright (C) 2014 Christian Hergert <chris@dronelabs.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

if [ -z "$1" ]; then
    echo "usage: record.sh capture_name"
    exit 1
fi

capture_name=$1

COMMANDS="malloc gmalloc gslice gobject"
N_THREADS="1 2 4 5 10 15 20 25"
SIZE="256"

mkdir -p samples/${capture_name}

for cmd in $COMMANDS; do
    filename="samples/${capture_name}/${cmd}.txt"
    rm -f ${filename}
    touch ${filename}
    for nthread in $N_THREADS; do
        for sz in $SIZE; do
            ./alloctest -c $cmd -s $sz -t $nthread -i 100000 | tee -a ${filename}
        done
    done
done

for cmd in $COMMANDS; do
    filename="samples/${capture_name}/${cmd}-tcmalloc.txt"
    rm -f ${filename}
    touch ${filename}
    for nthread in $N_THREADS; do
        for sz in $SIZE; do
            LD_PRELOAD=/usr/lib64/libtcmalloc_minimal.so.4 \
                ./alloctest -c $cmd -s $sz -t $nthread -i 100000 | tee -a ${filename}
        done
    done
done

for cmd in $COMMANDS; do
    gnuplot <<EOF
set term png size 1024,768
set out "samples/${capture_name}/${cmd}.png"
set title "${cmd} vs ${cmd}-tcmalloc"
set xlabel 'Thread Count'
set ylabel 'Alloc Cycles per Second (${SIZE} bytes)'
set style line 1 lw 4 lc rgb '#990042' ps 2 pt 6 pi 5
set style line 2 lw 3 lc rgb '#31f120' ps 2 pt 12 pi 3
plot [0:25] \
    'samples/${capture_name}/${cmd}.txt' using 4:5 with linespoints title '${cmd}', \
    'samples/${capture_name}/${cmd}-tcmalloc.txt' using 4:5 with linespoints title '${cmd}+tcmalloc'

EOF
done


gnuplot <<EOF
set term png size 1024,768
set out "samples/${capture_name}/gslice-vs-gmalloc.png"
set title "gslice vs gmalloc"
set xlabel 'Thread Count'
set ylabel 'Alloc Cycles per Second (${SIZE} bytes)'
set style line 1 lw 4 lc rgb '#990042' ps 2 pt 6 pi 5
set style line 2 lw 3 lc rgb '#31f120' ps 2 pt 12 pi 3
plot [0:25] \
    'samples/${capture_name}/gslice.txt' using 4:5 with linespoints title 'gslice', \
    'samples/${capture_name}/malloc.txt' using 4:5 with linespoints title 'malloc (glibc)', \
    'samples/${capture_name}/gmalloc.txt' using 4:5 with linespoints title 'gmalloc', \
    'samples/${capture_name}/gmalloc-tcmalloc.txt' using 4:5 with linespoints title 'gmalloc+tcmalloc'

EOF

gnuplot <<EOF
set term png size 1024,768
set out "samples/${capture_name}/gslice-vs-gobject.png"
set title "gslice vs g_object_new"
set xlabel 'Thread Count'
set ylabel 'Alloc Cycles per Second'
set style line 1 lw 4 lc rgb '#990042' ps 2 pt 6 pi 5
set style line 2 lw 3 lc rgb '#31f120' ps 2 pt 12 pi 3
plot [0:25] \
    'samples/${capture_name}/gslice.txt' using 4:5 with linespoints title 'gslice', \
    'samples/${capture_name}/gobject.txt' using 4:5 with linespoints title 'g_object_new'

EOF
