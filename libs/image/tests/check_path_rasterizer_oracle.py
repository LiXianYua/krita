#!/usr/bin/env python3
"""Contract checks for the independent Qt path-rasterizer oracle."""
import argparse
import re
import subprocess
import sys


def run(exe, *args):
    return subprocess.run([exe, *args], check=False, capture_output=True, text=True)


def parse_payload(text):
    lines = text.splitlines()
    if len(lines) < 3 or lines[0] != "KPR1":
        raise AssertionError(f"invalid header: {text[:80]!r}")
    name = lines[1]
    x, y, width, height = map(int, lines[2].split())
    payload = "".join(lines[3:])
    if not name or width <= 0 or height <= 0 or len(payload) != width * height * 2:
        raise AssertionError(f"invalid payload dimensions for {name!r}")
    if re.fullmatch(r"[0-9a-f]+", payload) is None:
        raise AssertionError(f"non-lowercase-hex payload for {name!r}")
    return name, (x, y, width, height), bytes.fromhex(payload)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oracle", required=True)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    listed = run(args.oracle, "--list")
    if listed.returncode:
        raise AssertionError(f"--list exited {listed.returncode}: {listed.stderr}")
    names = listed.stdout.splitlines()
    if not names or any(not n for n in names) or len(names) != len(set(names)):
        raise AssertionError("--list contains an empty or duplicate name")
    payloads = {}
    for name in names:
        result = run(args.oracle, "--case", name)
        if result.returncode:
            raise AssertionError(f"case {name} exited {result.returncode}: {result.stderr}")
        parsed_name, dimensions, data = parse_payload(result.stdout)
        if parsed_name != name:
            raise AssertionError(f"case name mismatch: {parsed_name!r} != {name!r}")
        payloads[name] = (dimensions, data)

    def data(name):
        return payloads[name][1]

    rects = [n for n in names if "aa_off_rect" in n and "nested" not in n]
    assert rects, "missing AA-off rectangle fixture"
    assert set(data(rects[0])) <= {0, 255}, "AA-off rectangle is not binary"
    odd = next(n for n in names if "nested_oddeven" in n)
    winding = next(n for n in names if "nested_winding" in n)
    assert data(odd) != data(winding), "fill rules produced identical masks"
    aa_off = next(n for n in names if "aa_off" in n and "rect" not in n)
    aa_on = next(n for n in names if "aa_on" in n)
    assert data(aa_off) != data(aa_on), "AA on/off produced identical masks"
    for name in names:
        if any(tag in name for tag in ("mirror", "chunk_boundary")):
            first = run(args.oracle, "--case", name)
            second = run(args.oracle, "--case", name)
            assert first.stdout == second.stdout, f"nondeterministic case {name}"
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, StopIteration, ValueError) as exc:
        print(f"oracle contract failure: {exc}", file=sys.stderr)
        raise SystemExit(1)
