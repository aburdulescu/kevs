#!/bin/sh

set -ex

# add testdata to corpus
mkdir -p testdata/corpus
./b/kevs_fuzz -merge=1 testdata/corpus/ testdata/not_valid/
./b/kevs_fuzz -merge=1 testdata/corpus/ testdata/valid/

# create temp corpus dir
rm -rf corpus
mkdir corpus
cp -r testdata/corpus/* corpus/

# run fuzzer
./b/kevs_fuzz -max_total_time=10 corpus

# generate coverage
rm -rf coverage.data coverage-out
./cov-merge.py default.profraw -o coverage.data
./cov-show.py b/kevs_fuzz -p coverage.data -s kevs.c

# merge corpus
rm -rf testdata/corpus
mkdir -p testdata/corpus
./b/kevs_fuzz -merge=1 testdata/corpus/ corpus
