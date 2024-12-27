#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description="Merge coverage profile")
    parser.add_argument(
        "profiles",
        help="Path to profiles",
        metavar="profile",
        nargs="+",
        type=str,
    )
    parser.add_argument(
        "-o",
        "--output",
        metavar="output_file",
        help="Where to write the output",
        type=str,
        default="coverage.profdata",
    )
    args = parser.parse_args()

    cmd_args = [
        "llvm-profdata-14",
        "merge",
        "-o",
        args.output,
        "-sparse",
    ]

    for p in args.profiles:
        cmd_args.append(os.path.abspath(p))

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
