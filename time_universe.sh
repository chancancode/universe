#!/bin/bash

BRANCH=`ruby -e "b = (%x{git branch 2> /dev/null}.grep(/^\*/).first || '').gsub(/^\* (.+)$/, '\1').chomp;print (b=='master')?'original':b"`
mkdir results/$BRANCH

for population in 5000
do
    #echo "Population $population (one thread):"
	#time universe -z 0 -f 10 -u 10000 -p $population -t 1 1> results/$BRANCH/$population.out 2> /dev/null
	#echo "Population $population (two threads):"
	#time universe -z 0 -f 10 -u 10000 -p $population -t 2 1> results/$BRANCH/$population.out 2> /dev/null
	echo "Population $population (three threads):"
	time universe -z 0 -f 10 -u 10000 -p $population -t 3 1> results/$BRANCH/$population.out 2> /dev/null
	diff results/original/$population.out results/$BRANCH/$population.out
done