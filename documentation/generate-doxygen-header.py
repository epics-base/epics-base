#!/usr/bin/env python3

"""
Generate a .rst file with the given header name,
under the doxygen/ directory,
composed of a single 'doxygenfile' instruction.

This includes the C/C++ API documentation of that header.
"""

import os
import sys

if len(sys.argv) != 2:
    print("Usage: generate-doxygen-header.py <name>")
    sys.exit(1)

os.makedirs("doxygen", exist_ok=True)

header_name = sys.argv[1]
doc_file = f"doxygen/{header_name}.rst"

with open(doc_file, "w") as f:
    print(f"Generating {doc_file}")
    underline = "=" * (len(header_name) + 2)
    f.write(
        f"""{header_name}.h
{underline}

.. doxygenfile:: {header_name}.h
"""
    )
