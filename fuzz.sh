#!/bin/sh

set -ex

# add testdata to corpus
mkdir -p testdata/corpus
./b/kevs_fuzz -detect_leaks=0 -merge=1 testdata/corpus/ testdata/not_valid/
./b/kevs_fuzz -detect_leaks=0 -merge=1 testdata/corpus/ testdata/valid/

# create temp corpus dir
rm -rf corpus
mkdir corpus
cp -r testdata/corpus/* corpus/

# run fuzzer
./b/kevs_fuzz -detect_leaks=0 -max_total_time=60 corpus

# merge corpus
rm -rf testdata/corpus
mkdir -p testdata/corpus
./b/kevs_fuzz -detect_leaks=0 -merge=1 testdata/corpus/ corpus
