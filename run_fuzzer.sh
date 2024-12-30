#!/bin/sh

# add testdata to corpus
mkdir -p testdata/corpus
./b/kevs_fuzz -merge=1 testdata/corpus/ testdata/not_valid/
./b/kevs_fuzz -merge=1 testdata/corpus/ testdata/valid/

# create temp corpus dir
rm -rf corpus
mkdir corpus
cp -r testdata/corpus/* corpus/

# run fuzzer
./b/kevs_fuzz corpus

# merge corpus
rm -rf testdata/corpus
mkdir -p testdata/corpus
./b/kevs_fuzz -merge=1 testdata/corpus/ corpus
