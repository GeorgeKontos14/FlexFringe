#!/bin/bash
#

#SBATCH --job-name="ensemble_runs"
#SBATCH --partition=compute
#SBATCH --time=04:00:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem-per-cpu=3G

module load 2024r1
module load cmake/3.27.7
module load fmt/9.1.0

mkdir build && cd build && cmake ..
make -j 12

cd ..

srun ./ensemble.sh
