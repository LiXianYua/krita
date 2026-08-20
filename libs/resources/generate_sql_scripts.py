#!/usr/bin/env python3
"""Generate an exact byte-for-byte C++ embedding of libs/resources/sql.qrc."""

from __future__ import annotations

import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


def cpp_bytes(data: bytes) -> str:
    if not data:
        return '""'
    chunks = []
    for offset in range(0, len(data), 24):
        part = data[offset : offset + 24]
        chunks.append('"' + "".join(f"\\x{byte:02x}" for byte in part) + '"')
    return "\n        ".join(chunks)


def load_entries(qrc_path: Path) -> list[tuple[str, bytes]]:
    root = ET.parse(qrc_path).getroot()
    entries: list[tuple[str, bytes]] = []
    seen: set[str] = set()
    for file_element in root.findall(".//file"):
        relative_path = (file_element.text or "").strip()
        if not relative_path:
            raise ValueError("sql.qrc contains an empty <file> entry")
        alias = file_element.get("alias", relative_path)
        if alias in seen:
            raise ValueError(f"duplicate SQL resource alias: {alias}")
        seen.add(alias)
        source_path = qrc_path.parent / relative_path
        entries.append((alias, source_path.read_bytes()))
    if not entries:
        raise ValueError("sql.qrc contains no SQL resources")
    return entries


def write_outputs(header_path: Path, source_path: Path, entries: list[tuple[str, bytes]]) -> None:
    header_path.parent.mkdir(parents=True, exist_ok=True)
    source_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(
        "#pragma once\n\n"
        "// Generated from libs/resources/sql.qrc; do not edit.\n"
        "const char *kisSqlScript(const char *alias);\n",
        encoding="utf-8",
    )

    lines = [
        '#include "KisSqlScripts.h"',
        "",
        "#include <cstring>",
        "",
        "const char *kisSqlScript(const char *alias)",
        "{",
        "    if (!alias) return nullptr;",
    ]
    for index, (alias, contents) in enumerate(entries):
        lines.extend(
            [
                f"    static const char script_{index}[] =",
                f"        {cpp_bytes(contents)};",
                f'    if (std::strcmp(alias, "{alias}") == 0) return script_{index};',
            ]
        )
    lines.extend(["    return nullptr;", "}", ""])
    source_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qrc", required=True, type=Path)
    parser.add_argument("--output-header", required=True, type=Path)
    parser.add_argument("--output-source", required=True, type=Path)
    args = parser.parse_args()
    write_outputs(args.output_header, args.output_source, load_entries(args.qrc))


if __name__ == "__main__":
    main()
