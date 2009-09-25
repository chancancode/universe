#!/bin/bash

for population in 10 100 500 $((10**3)) $((10**3 *5))
do
    echo "Population $population:"
	time universe -z 0 -f 10 -u 10000 -p $population 1> results/two-sectors/$population.out 2> /dev/null
	diff results/original/$population.out results/two-sectors/$population.out
done