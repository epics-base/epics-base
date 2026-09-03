#!/usr/bin/python3
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(
    prog="generate_module_version",
    description="generate code for epics module registration",
)
parser.add_argument("-n", "--name")
parser.add_argument("-v", "--version")

args = parser.parse_args()
filename = Path(f"{args.name}_registerModule.cpp")

CODE_TEMPLATE = f"""
/* THIS IS A GENERATED FILE. DO NOT EDIT! */
#ifndef INC_{filename.stem.upper()}_H
#define INC_{filename.stem.upper()}_H

extern "C" {{
#include <registerModules.h>
#include <compilerDependencies.h>
}}

#define EPICS_MODULE_NAME__ "{args.name}"
#define EPICS_MODULE_VERSION__ "{args.version}"

int __attribute__((used)) {args.name}_moduleAnchor = 0;
static int {args.name}_registerMyself __attribute__((used)) = registerModule(const_cast<char*>(EPICS_MODULE_NAME__), const_cast<char*>(EPICS_MODULE_VERSION__));

#undef EPICS_MODULE_NAME__
#undef EPICS_MODULE_VERSION__
#endif /* INC_{filename.stem.upper()}_H */
"""

with open(filename, "w") as f:
    f.write(CODE_TEMPLATE)
