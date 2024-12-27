#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(
        description="Generate coverage reports for given binary(ies)"
    )
    parser.add_argument(
        "binaries",
        help="Path to binaries",
        metavar="bin",
        nargs="+",
        type=str,
    )
    parser.add_argument(
        "-o",
        "--output",
        metavar="output_directory",
        help="Where to write the output",
        type=str,
        default="coverage-out",
    )
    parser.add_argument(
        "-p",
        "--profile",
        metavar="instrumented_profile",
        help="Path to instrumented profile file",
        type=str,
        default="coverage.profdata",
    )
    parser.add_argument(
        "--ignore",
        help="Skip source code files that match the given regex",
        metavar="regex",
        nargs="+",
        type=str,
    )
    parser.add_argument(
        "-s",
        "--sources",
        help="Show coverage only for this file/directory",
        metavar="path",
        nargs="+",
        type=str,
    )
    args = parser.parse_args()

    binaries = []
    for b in args.binaries:
        binaries.append(os.path.abspath(b))

    os.makedirs(args.output, exist_ok=True)

    cmd_args = [
        "llvm-cov-14",
        "show",
        "-instr-profile",
        args.profile,
        "-output-dir",
        args.output,
        "-format=html",
        "-show-branches=percent",
        "-show-line-counts=true",
        "-show-expansions=false",
        "-show-instantiations=true",
        "-show-regions=false",
    ]

    if args.ignore is not None:
        for ig in args.ignore:
            cmd_args.append("-ignore-filename-regex")
            cmd_args.append(ig)

    # need one binary without -object for SOURCES to work
    cmd_args.append(args.binaries[0])
    for b in args.binaries[1:]:
        cmd_args.append("-object")
        cmd_args.append(b)

    if args.sources is not None:
        for src in args.sources:
            cmd_args.append(src)

    print(" ".join(cmd_args))

    res = subprocess.run(
        cmd_args,
        stdout=sys.stdout,
        stderr=sys.stderr,
        text=True,
    )

    if res.returncode != 0:
        sys.exit(res.returncode)


if __name__ == "__main__":
    main()
