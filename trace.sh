#!/bin/sh

set -ex

sudo perf probe -q -x ./kevs --add="scan_*" --add="scan_*%return"
sudo perf probe -q -x ./kevs --add="parse_*" --add="parse_*%return"
sudo perf record -e "probe_kevs:*" ./kevs $@ || true
sudo perf probe -q --del "probe_kevs:*"
sudo perf script -s trace.py
