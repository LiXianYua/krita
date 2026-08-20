# pk/string

`pk/string` provides the Qt-free `PkString` UTF-16 COW value type. Its only
in-tree implementation dependency is `pk/container`; the standalone target is
built and tested by `pk/string/CMakeLists.txt` and `pk/string/tests/run_tests.sh`.

## Case-conversion graft handoff

R-31 added default `toLower()` / `toUpper()` call-shape coverage for these real
Krita consumers:

- `plugins/impex/csv/csv_loader.cpp:75`
- `plugins/impex/csv/csv_saver.cpp:78`
- `libs/resources/KisResourceModel.cpp:1116`

The persistent check is `pk/string/tests/graft/case_callshape.cpp`, run by
`pk/string/tests/graft/graft_check.sh`. It reproduces those expressions and
their Qt 5.15.7 results, but it is intentionally a driver rather than a claim
that the real translation units compile against `PkString`.

Compiling the real CSV translation units currently crosses the `pk/string`
module boundary into `QObject`, `QIODevice`, `QVector`, and `KisDocument`.
Compiling `KisResourceModel.cpp` additionally requires the resource-model
target graph. Those dependencies are owned by later migration work and were
outside the R-31 `pk/string` lock.

When those dependencies have Qt-free owners, replace or supplement the driver
with syntax/build checks of the real translation units. Until then, treat a
green case-callshape driver as evidence for the `PkString` expressions only,
not for the surrounding CSV or resource-model dependency closure.

Unicode case-table provenance and regeneration instructions live in
`pk/string/unicode_case/README.md`.
