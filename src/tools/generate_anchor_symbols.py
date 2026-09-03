#!/usr/bin/python3
import argparse
from pathlib import Path


parser = argparse.ArgumentParser(
    prog="generate_anchor_symbols",
    description="generate the weak symbols for epics module registration",
)

parser.add_argument(
    "-p",
    "--prod",
    required=True,
)
parser.add_argument("libs", nargs="+")

args = parser.parse_args()
filename = Path(f"{args.prod}_weakAnchor.cpp")

symbols = [
    f"extern int {l}_moduleAnchor;static __attribute__((used)) int {l}_anchorHolder = {l}_moduleAnchor;"
    for l in args.libs
]

CODE_TEMPLATE = f"""
/* THIS IS A GENERATED FILE. DO NOT EDIT! */
#ifndef INC_{filename.stem.upper()}_H
#define INC_{filename.stem.upper()}_H

{"\n".join(symbols)}

#endif /* INC_{filename.stem.upper()}_H */
"""

with open(filename, "w") as f:
    f.write(CODE_TEMPLATE)
