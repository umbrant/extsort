#!/bin/bash

for i in 128 256 384 512
do
	for j in 512 1024 2048 4096
	do
		rm -f bar*.dat foo*.dat
		echo "Executing extsort $i $j"
		./extsort random_1G.dat $i $j > ${i}_${j}.log
	done
done
