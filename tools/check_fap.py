#!/usr/bin/env python3
"""ESL Zap build check: every undefined symbol of a .fap must exist in the
firmware API table (api_symbols.csv), otherwise the app fails to load.
Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
Usage: check_fap.py app.fap path/to/api_symbols.csv"""
import csv, sys
from elftools.elf.elffile import ELFFile

fap, api_csv = sys.argv[1:3]
api = set()
with open(api_csv) as f:
    for row in csv.reader(f):
        if len(row) > 2 and row[0] in ("Function", "Variable") and row[1] == "+":
            api.add(row[2])
elf = ELFFile(open(fap, "rb"))
missing = []
for sym in elf.get_section_by_name(".symtab").iter_symbols():
    if sym["st_shndx"] == "SHN_UNDEF" and sym.name:
        name = sym.name[len("__wrap_"):] if sym.name.startswith("__wrap_") else sym.name
        if name not in api and sym.name not in api:
            missing.append(sym.name)
if missing:
    print("MISSING SYMBOLS in %s:" % fap, ", ".join(sorted(set(missing))))
    sys.exit(1)
print("OK %s: all symbols available" % fap)
