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
N_THREADS="1 2 4 5 10 25 50 75 100"
SIZE="32 128 256 512 1024 2048"

mkdir -p samples
fname="samples/${capture_name}.txt"
rm -f ${fname}
touch ${fname}

for cmd in $COMMANDS; do
    for nthread in $N_THREADS; do
        for sz in $SIZE; do
            ./alloctest -c $cmd -s $sz -t $nthread -i 100000 | tee -a ${fname}
        done
    done
done

for cmd in $COMMANDS; do
    for nthread in $N_THREADS; do
        for sz in $SIZE; do
            LD_PRELOAD=/usr/lib64/libtcmalloc_minimal.so.4 ./alloctest -c $cmd -s $sz -t $nthread -i 100000 | tee -a ${fname}
        done
    done
done
