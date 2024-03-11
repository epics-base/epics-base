#!/usr/bin/env python3

"""
Generate a .rst file with the given title,
composed of a single toctree that includes the given headers.
"""

import sys

if len(sys.argv) < 3:
    print("Usage: generate-api-index.py <file> <title> <headers>")
    sys.exit(1)

index_file = sys.argv[1]
title = sys.argv[2]
headers = sys.argv[3:]

with open(index_file, "w") as f:
    print(f"Generating {index_file}")
    underline = "=" * len(title)
    f.write(
        f"""{title}
{underline}

.. toctree::
   :maxdepth: 1
   :caption: {title}

"""
    )

    for header in headers:
        f.write(f"   {header}\n")
