#!/bin/sh

main_corpus_dir=testdata/corpus
fuzz_out_dir=fuzz-out
temp_corpus_dir=${fuzz_out_dir}/corpus
coverage_dir=${fuzz_out_dir}/coverage-out
coverage_raw_profile=${fuzz_out_dir}/coverage.profraw
coverage_profile=${fuzz_out_dir}/coverage.data
max_time=30

set -ex

# add testdata to corpus
./b/kevs_fuzz -create_missing_dirs=1 -merge=1 ${main_corpus_dir}/ testdata/not_valid/
./b/kevs_fuzz -create_missing_dirs=1 -merge=1 ${main_corpus_dir}/ testdata/valid/

rm -rf ${fuzz_out_dir}

# create temp corpus dir
mkdir -p ${temp_corpus_dir}
cp -r ${main_corpus_dir}/* ${temp_corpus_dir}/

# run fuzzer
LLVM_PROFILE_FILE=${coverage_raw_profile} ./b/kevs_fuzz -max_total_time=${max_time} -create_missing_dirs=1 -artifact_prefix=${fuzz_out_dir} ${temp_corpus_dir}

# generate coverage
./cov-merge.py ${coverage_raw_profile} -o ${coverage_profile}
./cov-show.py b/kevs_fuzz -p ${coverage_profile} -s kevs.c -o ${coverage_dir}

# merge corpus
rm -rf ${main_corpus_dir}
./b/kevs_fuzz -create_missing_dirs=1 -merge=1 ${main_corpus_dir}/ ${temp_corpus_dir}
