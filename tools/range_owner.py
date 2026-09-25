#!/usr/bin/env python3
"""Which unit, if any, claims a section range in config/G2ME01/splits.txt.

    python3 tools/range_owner.py .text 0x80079BE4 0x8007A53C

Prints the owning unit, or UNCLAIMED. Pass a fifth argument to also list overlapping ranges. Use it
before carving a range for a new unit - the usual reasons a port cannot land are that the bytes are
already claimed, or that the range does not end on a function boundary (see tools/range_bounds.py).

From lane W16, 2026-09-25.
"""
