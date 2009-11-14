#!/bin/bash

BRANCH=`ruby -e "b = (%x{git branch 2> /dev/null}.grep(/^\*/).first || '').gsub(/^\* (.+)$/, '\1').chomp;print (b=='master')?'original':b"`
mkdir results/$BRANCH

for population in 5000
do
    echo "Population $population (one process):"
	time universe -z 0 -f 10 -u 10000 -p $population -n 1 1> results/$BRANCH/$population-1.out 2> /dev/null
	diff results/original/$population.out results/$BRANCH/$population-1.out
	echo "Population $population (two processes):"
	time universe -z 0 -f 10 -u 10000 -p $population -n 2 1> results/$BRANCH/$population-2.out 2> /dev/null
	diff results/original/$population.out results/$BRANCH/$population-2.out
	echo "Population $population (three processes):"
	time universe -z 0 -f 10 -u 10000 -p $population -n 3 1> results/$BRANCH/$population-3.out 2> /dev/null
	diff results/original/$population.out results/$BRANCH/$population-3.out
done