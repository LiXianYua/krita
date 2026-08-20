# PkString Unicode case data

`CaseMapping-13.0.0.txt` is the vendored, compact input for `PkString`'s
default `toLower()` / `toUpper()` behavior. It contains Unicode 13.0 simple
case mappings plus only the unconditional entries from full SpecialCasing.
Locale- and context-conditioned rules are intentionally excluded; locale
tailoring belongs to the later `QLocale` replacement.

The original Unicode 13.0 inputs are:

- `https://www.unicode.org/Public/13.0.0/ucd/UnicodeData.txt`, SHA-256
  `bdbffbbfc8ad4d3a6d01b5891510458f3d36f7170422af4ea2bed3211a73e8bb`
- `https://www.unicode.org/Public/13.0.0/ucd/SpecialCasing.txt`, SHA-256
  `6424312f1dc39b22e0ff9c0ffb13dfad424d9b03e6a6dc6bca941f6bf5ef1ffd`

Regenerate after downloading those exact files:

```sh
./import_ucd.py UnicodeData.txt SpecialCasing.txt CaseMapping-13.0.0.txt
./generate_case_data.py CaseMapping-13.0.0.txt PkUnicodeCaseData.h
```

`import_ucd.py` rejects any source whose hash differs. The generated header
stores results as UTF-16 units, so runtime conversion does not depend on ICU,
the C locale, or the host standard library's Unicode version. The oracle
exhaustively compares every Unicode scalar against the Qt 5.15.7 build whose
private table declares `UNICODE_DATA_VERSION QChar::Unicode_13_0`.

The data is distributed under the Unicode Data Files and Software License in
`LICENSE.UNICODE`.
