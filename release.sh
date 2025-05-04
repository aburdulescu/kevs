#!/bin/sh

[ $# -lt 1 ] && echo "error: need tag" && exit 1
[ -z "$ZIG" ] && echo "error: ZIG env var not set" && exit 1

version=$1

set -ex

git checkout "$version"

rm -rf zig-out/
$ZIG build -Doptimize=ReleaseFast

rm -rf kevs-bin-"$version" kevs-bin-"$version".zip
mkdir -p kevs-bin-"$version"
cp -r zig-out/ReleaseFast/* kevs-bin-"$version"
zip -r kevs-bin-"$version".zip kevs-bin-"$version"

rm -rf kevs-src-"$version" kevs-src-"$version".zip
mkdir -p kevs-src-"$version"
cp src/c/kevs.* kevs-src-"$version"/
zip -r kevs-src-"$version".zip kevs-src-"$version"
