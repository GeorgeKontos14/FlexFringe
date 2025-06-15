#!/bin/bash

for j in {1..10}; do
  echo "Iteration $j"
  mkdir rwalks/walk_runs$j
  for i in {1..20}; do
    echo "Running on file $i"
    ./build/flexfringe --ini ini/random.ini --mode random_walk --nrestimators 100 ./data/stamina_split/${i}_training.txt.dat --outputfile ./rwalks/walk_runs$j/rw$i
    ./build/flexfringe --ini ini/random.ini --mode predict --predicttype 1 --ensemblefile ./rwalks/walk_runs$j/rw$i.final.random.json ./data/stamina_split/${i}_test.txt.dat
    ./build/flexfringe --ini ini/random.ini --mode predict --predictindividual 1 --ensemblefile ./rwalks/walk_runs$j/rw$i.final.random.json ./data/stamina_split/${i}_test.txt.dat
  done
done
