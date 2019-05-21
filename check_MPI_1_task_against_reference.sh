#!/bin/bash

#
# ATTENTION: need to be in an active slurm allocation!
#

module purge
module load intel intel-mpi intel-mkl boost eigen zlib
module list

EXE=./src/mpi_gibbs

# COMPILATION
cd ./src
B='-B'
B=''
make $B -f Makefile || exit 1;
cd ..

if [ ! -f $EXE ]; then
    echo Fatal: binary $EXE not found!
    exit
fi

# DATASET
datadir=./test/data
dataset=uk10k_chr1_1mb
phen=test
S="1.0,0.1"

echo 
echo "======================================"
echo "        RUNNING THE APPLICATION ON:   "
echo "datadir:" $datadir
echo "dataset:" $dataset
echo "S      :" $S
echo "======================================"
echo

CL=20
SEED=5
SR=0
SM=0
NM=100

# If you change those, do not expect compatibility
N=1
TPN=1 

echo "@@@ Solution reading from  BED file @@@"
sol=bed
rm $sol.csv
rm $sol.bet
srun -N $N --ntasks-per-node=$TPN $EXE --mpibayes bayesMPI --bfile $datadir/$dataset --pheno $datadir/${phen}.phen --chain-length $CL --burn-in 0 --thin 1 --mcmc-samples $sol.csv --mcmc-betas $sol.bet --seed $SEED --shuf-mark $SM --mpi-sync-rate $SR --S $S --read-from-bed-file  --number-markers $NM
echo; echo

echo "@@@ Solution reading from SPARSE files @@@"
sol=sparse
rm $sol.csv
rm $sol.bet
srun -N $N --ntasks-per-node=$TPN $EXE --mpibayes bayesMPI --bfile $datadir/$dataset --pheno $datadir/${phen}.phen --chain-length $CL --burn-in 0 --thin 1 --mcmc-samples $sol.csv --mcmc-betas $sol.bet --seed $SEED --shuf-mark $SM --mpi-sync-rate $SR --S $S --marker-blocks-file $datadir/${dataset}.blk_1 --number-markers $NM

echo 
echo
echo "@@@ Official (sequential) solution (reading from BED file) @@@"
echo
$EXE --bayes bayesMmap --bfile $datadir/$dataset --pheno $datadir/${phen}.phen --chain-length $CL --burn-in 0 --thin 1 --mcmc-samples ref.csv --shuf-mark $SM --seed $SEED --S $S --number-markers $NM
