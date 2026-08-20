#!/usr/bin/env python3
"""Generate an exact byte-for-byte C++ embedding from sql/manifest.tsv."""

from __future__ import annotations

import argparse
from pathlib import Path, PurePosixPath


def cpp_bytes(data: bytes) -> str:
    if not data:
        return '""'
    chunks = []
    for offset in range(0, len(data), 24):
        part = data[offset : offset + 24]
        chunks.append('"' + "".join(f"\\x{byte:02x}" for byte in part) + '"')
    return "\n        ".join(chunks)


def _canonical_relative_path(value: str, field: str, line_number: int) -> PurePosixPath:
    if not value or value != value.strip():
        raise ValueError(f"manifest line {line_number}: {field} is empty or has outer whitespace")
    if "\\" in value or value.startswith(("/", ":")) or "\0" in value:
        raise ValueError(f"manifest line {line_number}: invalid {field}: {value!r}")
    path = PurePosixPath(value)
    if path.as_posix() != value or any(part in ("", ".", "..") for part in path.parts):
        raise ValueError(f"manifest line {line_number}: non-canonical {field}: {value!r}")
    return path


def load_entries(manifest_path: Path) -> list[tuple[str, bytes]]:
    entries: list[tuple[str, bytes]] = []
    seen: set[str] = set()
    manifest_root = manifest_path.parent.resolve()
    for line_number, raw_line in enumerate(manifest_path.read_text(encoding="utf-8").splitlines(), 1):
        if not raw_line or raw_line.startswith("#"):
            continue
        fields = raw_line.split("\t")
        if len(fields) != 2:
            raise ValueError(f"manifest line {line_number}: expected alias<TAB>source")
        alias, relative_path = fields
        _canonical_relative_path(alias, "alias", line_number)
        source_relative = _canonical_relative_path(relative_path, "source", line_number)
        if source_relative.suffix != ".sql":
            raise ValueError(f"manifest line {line_number}: source is not an .sql file")
        if alias in seen:
            raise ValueError(f"duplicate SQL resource alias: {alias}")
        seen.add(alias)
        source_path = (manifest_root / Path(*source_relative.parts)).resolve()
        try:
            source_path.relative_to(manifest_root)
        except ValueError as error:
            raise ValueError(f"manifest line {line_number}: source escapes manifest directory") from error
        contents = source_path.read_bytes()
        if b"\0" in contents:
            raise ValueError(f"manifest line {line_number}: SQL source contains a NUL byte")
        entries.append((alias, contents))
    if not entries:
        raise ValueError("SQL manifest contains no entries")
    return entries


def write_outputs(header_path: Path, source_path: Path, entries: list[tuple[str, bytes]]) -> None:
    header_path.parent.mkdir(parents=True, exist_ok=True)
    source_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(
        "#pragma once\n\n"
        "// Generated from libs/resources/sql/manifest.tsv; do not edit.\n"
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
                f"    if (std::strcmp(alias, {cpp_bytes(alias.encode('utf-8'))}) == 0) return script_{index};",
            ]
        )
    lines.extend(["    return nullptr;", "}", ""])
    source_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output-header", required=True, type=Path)
    parser.add_argument("--output-source", required=True, type=Path)
    args = parser.parse_args()
    write_outputs(args.output_header, args.output_source, load_entries(args.manifest))


if __name__ == "__main__":
    main()
