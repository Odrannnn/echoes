#!/usr/bin/env python3
"""Check that a proposed section range starts and ends on real symbols in the retail binary.

    python3 tools/range_bounds.py 0x80079BE4 0x8007A53C

Lists the symbols inside the range and prints what lies exactly at each boundary. A split that cuts
a function in half cannot be reproduced by any source, so check this before claiming a range.

From lane W16, 2026-09-25.
"""
