#!/bin/bash

for population in 10 100 500 $((10**3)) $((10**3 *5))
do
    BRANCH=`ruby -e "b = (%x{git branch 2> /dev/null}.grep(/^\*/).first || '').gsub(/^\* (.+)$/, '\1').chomp;print (b=='master')?'original':b"`
    echo "Population $population:"
	time universe -z 0 -f 10 -u 10000 -p $population 1> results/$BRANCH/$population.out 2> /dev/null
	diff results/original/$population.out results/$BRANCH/$population.out
done