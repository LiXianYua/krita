#!/usr/bin/env python3
"""Extract Unicode 13.0 default full lower/upper mappings into a compact input.

The source files are not needed at build time.  Their fixed hashes make an
import reproducible and prevent silently regenerating from a newer UCD.
"""

from __future__ import annotations

import hashlib
import pathlib
import sys


UNICODE_DATA_SHA256 = "bdbffbbfc8ad4d3a6d01b5891510458f3d36f7170422af4ea2bed3211a73e8bb"
SPECIAL_CASING_SHA256 = "6424312f1dc39b22e0ff9c0ffb13dfad424d9b03e6a6dc6bca941f6bf5ef1ffd"


def check_hash(path: pathlib.Path, expected: str) -> None:
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(f"{path}: expected sha256 {expected}, got {actual}")


def parse_seq(text: str) -> tuple[int, ...]:
    return tuple(int(token, 16) for token in text.split())


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: import_ucd.py UnicodeData.txt SpecialCasing.txt CaseMapping-13.0.0.txt", file=sys.stderr)
        return 2

    unicode_data = pathlib.Path(sys.argv[1])
    special_casing = pathlib.Path(sys.argv[2])
    output = pathlib.Path(sys.argv[3])
    check_hash(unicode_data, UNICODE_DATA_SHA256)
    check_hash(special_casing, SPECIAL_CASING_SHA256)

    lower: dict[int, tuple[int, ...]] = {}
    upper: dict[int, tuple[int, ...]] = {}
    for line in unicode_data.read_text(encoding="utf-8").splitlines():
        fields = line.split(";")
        cp = int(fields[0], 16)
        if fields[12]:
            upper[cp] = (int(fields[12], 16),)
        if fields[13]:
            lower[cp] = (int(fields[13], 16),)

    for raw_line in special_casing.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = [field.strip() for field in line.split(";")]
        cp = int(fields[0], 16)
        condition = fields[4]
        if condition:
            # Default QString conversion has no language parameter and Qt 5.15
            # does not apply context/locale-tailored SpecialCasing here.
            continue
        lower[cp] = parse_seq(fields[1])
        upper[cp] = parse_seq(fields[3])

    for mapping in (lower, upper):
        for cp in list(mapping):
            if mapping[cp] == (cp,):
                del mapping[cp]

    lines = [
        "# Unicode CaseMapping-13.0.0",
        "# Derived from UnicodeData.txt simple mappings plus unconditional",
        "# SpecialCasing.txt full mappings. L=lower, U=upper.",
    ]
    for direction, mapping in (("L", lower), ("U", upper)):
        for cp, result in sorted(mapping.items()):
            lines.append(f"{direction};{cp:06X};{' '.join(f'{value:06X}' for value in result)}")
    output.write_text("\n".join(lines) + "\n", encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
