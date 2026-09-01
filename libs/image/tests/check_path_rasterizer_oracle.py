#!/usr/bin/env python3
"""Contract checks for the independent Qt path-rasterizer oracle."""
import argparse
import hashlib
import re
import subprocess
import sys


def run(exe, *args):
    return subprocess.run([exe, *args], check=False, capture_output=True, text=True)


def parse_payload(text):
    if not text.endswith("\n"):
        raise AssertionError("payload is missing the required final newline")
    lines = text.split("\n")
    if len(lines) != 5 or lines[0] != "KPR1" or lines[4] != "":
        raise AssertionError(f"invalid header: {text[:80]!r}")
    name = lines[1]
    x, y, width, height = map(int, lines[2].split())
    payload = lines[3]
    if not name or width <= 0 or height <= 0 or len(payload) != width * height * 2:
        raise AssertionError(f"invalid payload dimensions for {name!r}")
    if re.fullmatch(r"[0-9a-f]+", payload) is None:
        raise AssertionError(f"non-lowercase-hex payload for {name!r}")
    return name, (x, y, width, height), bytes.fromhex(payload)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oracle", required=True)
    ap.add_argument("--candidate")
    ap.add_argument("--mode", choices=("fill", "stroke", "all"))
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test == bool(args.candidate):
        ap.error("select exactly one of --self-test or --candidate")
    if args.candidate and args.mode is None:
        ap.error("--candidate requires --mode")

    list_option = "--list"
    if args.candidate and args.mode != "all":
        list_option = f"--list-{args.mode}"
    listed = run(args.oracle, list_option)
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

    if args.candidate:
        candidate_list = run(args.candidate, list_option)
        if candidate_list.returncode:
            raise AssertionError(
                f"candidate --list exited {candidate_list.returncode}: "
                f"{candidate_list.stderr}"
            )
        candidate_names = candidate_list.stdout.splitlines()
        if (not candidate_names or any(not n for n in candidate_names)
                or len(candidate_names) != len(set(candidate_names))):
            raise AssertionError("candidate --list contains an empty or duplicate name")
        missing = [name for name in names if name not in candidate_names]
        unexpected = [name for name in candidate_names if name not in payloads]
        if missing or unexpected:
            raise AssertionError(
                f"candidate {args.mode} fixture set differs from oracle: "
                f"missing={missing!r} unexpected={unexpected!r}"
            )

        for name in names:
            result = run(args.candidate, "--case", name)
            if result.returncode:
                raise AssertionError(
                    f"candidate case {name} exited {result.returncode}: {result.stderr}"
                )
            actual_name, actual_dimensions, actual = parse_payload(result.stdout)
            expected_dimensions, expected = payloads[name]
            if actual_name != name:
                raise AssertionError(
                    f"candidate case name mismatch: {actual_name!r} != {name!r}"
                )
            if actual_dimensions != expected_dimensions:
                raise AssertionError(
                    f"case={name} expected_bounds={expected_dimensions} "
                    f"actual_bounds={actual_dimensions}"
                )
            if actual != expected:
                x, y, width, _ = expected_dimensions
                mismatch = next(i for i, pair in enumerate(zip(expected, actual))
                                if pair[0] != pair[1])
                row = mismatch // width
                column = mismatch % width
                row_start = row * width
                expected_row = expected[row_start:row_start + width]
                actual_row = actual[row_start:row_start + width]
                raise AssertionError(
                    f"case={name} x={x + column} y={y + row} "
                    f"expected={expected[mismatch]} actual={actual[mismatch]} "
                    f"row_digest_expected={hashlib.sha256(expected_row).hexdigest()} "
                    f"row_digest_actual={hashlib.sha256(actual_row).hexdigest()}"
                )
        print(f"{args.mode} parity: {len(names)} cases, byte-identical")
        return 0

    def data(name):
        return payloads[name][1]

    rects = [n for n in names if "aa_off_rect" in n and "nested" not in n]
    assert rects, "missing AA-off rectangle fixture"
    assert set(data(rects[0])) <= {0, 255}, "AA-off rectangle is not binary"
    odd = next(n for n in names if "nested_oddeven" in n)
    winding = next(n for n in names if "nested_winding" in n)
    assert data(odd) != data(winding), "fill rules produced identical masks"
    aa_off = "aa_off_curve"
    aa_on = "aa_on_curve"
    assert aa_off in payloads and aa_on in payloads, "missing matched AA curve fixtures"
    assert data(aa_off) != data(aa_on), "AA on/off produced identical masks"
    for family in (("cap_flat", "cap_square", "cap_round"),
                   ("join_miter_limit_8", "join_bevel", "join_round"),
                   ("style_solid", "style_dash", "style_dot",
                    "style_dash_dot", "style_dash_dot_dot",
                    "style_custom_offset")):
        assert all(name in payloads for name in family), f"missing semantic family {family}"
        assert len({data(name) for name in family}) == len(family), \
            f"semantic family produced duplicate masks: {family}"
    assert data("join_miter_limit_1") != data("join_miter_limit_8"), \
        "miter limits produced identical masks"
    assert data("join_svg_miter_limit_1") != data("join_miter_limit_1"), \
        "SvgMiterJoin and MiterJoin clipping produced identical masks"
    assert data("zero_width_cosmetic") == data("unit_width"), \
        "zero-width cosmetic stroke did not normalize to one pixel"
    assert data("cosmetic_half_binary") != data("cosmetic_half_aa"), \
        "subpixel cosmetic AA on/off produced identical masks"
    assert data("cosmetic_half_aa") != data("unit_width"), \
        "subpixel cosmetic opacity did not differ from unit width"
    assert data("cosmetic_dash_flat") != data("cosmetic_dash_square"), \
        "cosmetic dash caps produced identical masks"
    assert "cosmetic_dash_flat_binary" in payloads, \
        "missing aliased cosmetic dash fixture"
    assert any(data("cosmetic_dash_flat_binary")), \
        "aliased cosmetic dash fixture produced an empty mask"
    assert set(data("cosmetic_dash_flat_binary")) <= {0, 255}, \
        "aliased cosmetic dash fixture is not binary"
    assert data("cosmetic_dash_flat_binary") != data("cosmetic_dash_flat"), \
        "aliased and antialiased cosmetic dash fixtures produced identical masks"
    assert "cosmetic_dash_offset_pos_limit" in payloads \
        and "cosmetic_dash_offset_neg_limit" in payloads, \
        "missing supported cosmetic dash-offset boundary fixtures"
    assert any(data("cosmetic_dash_offset_pos_limit")) \
        and any(data("cosmetic_dash_offset_neg_limit")), \
        "supported cosmetic dash-offset boundary fixture produced an empty mask"
    assert data("cosmetic_dash_offset_pos_limit") \
        != data("cosmetic_dash_offset_neg_limit"), \
        "positive and negative cosmetic dash-offset limits produced identical masks"
    assert "wide_dash_binary" in payloads, \
        "missing wide binary dash fixture"
    assert any(data("wide_dash_binary")), \
        "wide binary dash fixture produced an empty mask"
    assert set(data("wide_dash_binary")) <= {0, 255}, \
        "wide binary dash fixture is not binary"
    assert data("wide_dash_binary") != data("style_custom_offset"), \
        "wide binary and antialiased dash fixtures produced identical masks"
    assert any(data("cosmetic_closed_dash")), \
        "closed cosmetic dash fixture produced an empty mask"
    assert data("closed_flat_cap") == data("closed_round_cap"), \
        "closed subpath incorrectly applied end caps"
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
