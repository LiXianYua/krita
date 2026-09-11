# pk/render

`PkPainter` is a Qt-free command producer. Every effective state mutation and draw
submits one typed `PkPaintCommand` to the mandatory `PkPainterBackend`. `submit()` is
synchronous: its `const PkPaintCommand &` refers to a temporary that is valid only for
the duration of the call. A backend that defers or queues work must copy the command.
Command payloads are values (including COW path/image values), so copied commands
remain valid after the caller destroys or mutates the source values. `save` and
`restore` track the complete local pen/brush/transform/render-hint state; restoring an
empty stack does not submit an invalid restore command.

The originally measured surface was limited to Crop, Knife, Karbon Calligraphy and
Smart Patch. That list is **no longer accurate**: `PkPainter` has since grown
`setFont`/`drawText`/`drawPixmap`/`drawTiledPixmap`/`setCompositionMode`/`setOpacity`/
`fillTexturePath`, `PkGradient.h` lives here, `PkSvgRasterizer` renders through
`thirdparty/nanosvg`, and a real rasterizer (`libs/flake/PkImageRasterBackend.cpp`,
compiled into `pkrender` by this file's own `CMakeLists.txt`) fills and strokes paths.

**What is absent is a Qt backend** (by design, see the section at the end of this file),
and **not every declared command has a raster implementation**. The command-by-command
truth is `## Backend command coverage` below; that section, not this paragraph, is the
surface of record.

## Measured API surface

| Family | Overloads |
|---|---|
| State stack | `save()`, `restore()` |
| Pen | `PkPen(Qt::PenStyle)`, `pen()`, `setPen(const PkPen &)`, `setPen(const PkColor &, qreal)`, `setPen(Qt::PenStyle)` |
| Brush | `brush()`, `setBrush(const PkBrush &)`, `setBrush(const PkColor &)`, `setBrush(Qt::BrushStyle)` |
| Transform | `transform()`, `setTransform(const PkTransform &, bool)` |
| Render hints | `PkPainter::RenderHint::Antialiasing` / `PkPainter::Antialiasing` (`0x01`), `setRenderHint(unsigned, bool)`, `setRenderHints(unsigned, bool)` |
| Clip | `setClipRect(const PkRectF &, Qt::ClipOperation)` |
| Line | `drawLine(const PkLineF &)`, `drawLine(const PkPointF &, const PkPointF &)` |
| Rectangle | `drawRect(const PkRectF &)` |
| Ellipse | `drawEllipse(const PkRectF &)`, `drawEllipse(const PkPointF &, qreal, qreal)` |
| Arc | `drawArc(const PkRectF &, int, int)` |
| Path/polygon | `drawPath(const PkPainterPath &)`, `drawPolygon(const PkPolygonF &)` |
| Image | `drawImage(const PkRectF &, const PkImage &)` |

`PkPen(Qt::PenStyle)`, `setPen(Qt::PenStyle)`, and `setBrush(Qt::BrushStyle)` construct
fresh black default values of the requested style, matching the measured QPen/QPainter
overload semantics.
Combined transforms are tracked as `newTransform * currentTransform`, while the
submitted command preserves the original transform plus its `combine` flag for the
backend adapter.

No Qt backend or rasterizer lives in core. A transition adapter that consumes these
commands belongs above `pk/render` (currently the `libs/flake` boundary).

## Backend command coverage

`pk/render/PkPaintCommand.h` defines 27 command types. `PkImageRasterBackend::submit`
(`libs/flake/PkImageRasterBackend.cpp`) implements 21 of them and throws
`std::logic_error("PkImageRasterBackend does not support this paint command")` for the
rest. The table below is the **diff set** R-51 measured: every count is
call sites verified by receiver type (not by grep), over the retained range
(`libs/ plugins/ pk/ sdk/`, excluding `tests/`, `benchmarks/` and `oracle/` at any depth).

| Command | Raster backend | Live call sites | Files | Batch |
|---|---|---|---|---|
| `PkDrawPolygonCommand` | **implemented 2026-09-11** (`addPolygon` + `closeSubpath`) | 13 | 4 | R-51 |
| `PkDrawEllipseCommand` | **implemented 2026-09-11** (`addEllipse`) | 10 | 6 | R-51 |
| `PkDrawArcCommand` | throws | 2 | 1 | R-51/1b |
| `PkDrawTextAtPointCommand` | throws | 1 | 1 | R-51/2 |
| `PkDrawTextInRectCommand` | throws | 1 | 1 | R-51/2 |
| `PkSetFontCommand` | throws | 0 | 0 | R-51/2 |

`drawPolygon` and `drawEllipse` are Qt 5.15.7-pixel-identical to building the
corresponding `PkPainterPath` and routing it through the existing fill/stroke
machinery — verified case-by-case by `oracle/run_shape_primitive.sh`: **36 cases, of
which 29 are discriminating and 7 are degenerate/no-op** (integer / non-integer /
out-of-bounds / negative-size rects, single-point and two-point polygons,
self-intersecting polygons, pen+brush combinations). The 7 no-op cases are kept as
"degenerate input draws nothing, on both sides" assertions; they are not independent
evidence of drawing correctness and the test binary prints the split so the number
cannot be misread.

The one empirical trap: `QPainter::drawPolygon` **closes the subpath** before
stroking; `PkPainterPath::addPolygon` produces an *open* one. Omitting `closeSubpath()`
changes 74 / 76 / 106 pixels for the 4-vertex, self-intersecting and explicitly-closed
quads (32x32 ARGB32, per-pixel packed-value compare) and turns 6 of the 36 cases red.

**Registered gaps, with their blockers:**

- **`drawArc` is blocked on `pk/geometry`, not on this module.** Qt's `drawArc` is
  `arcMoveTo(rect, a/16.0)` + `arcTo(rect, a/16.0, span/16.0)`, stroke only (the brush
  is ignored — measured). `PkPainterPath::arcMoveTo` exists but is **private**
  (`pk/geometry/PkPainterPath.h`, marked "for addRoundedRect only"); real Qt exposes
  `QPainterPath::arcMoveTo` as public API. Without it, calling `arcTo` on an empty path
  starts from `(0, 0)` (`PkPainterPath::lineTo` moves to the origin on an empty path),
  which is not what Qt does. Making it public is a one-line move in `pk/geometry`,
  whose lock is currently held by another task.
- **`drawText`/`setFont`** need glyph rasterisation and layout. `pk/font` already has
  `PkFontRasterizer` (FreeType/HarfBuzz/Raqm all available), but Qt's text path goes
  through the raster engine's glyph cache and `QTextLayout`, so parity is a batch of
  its own.
- **`setCompositionMode`** accepts only `SourceOver`/`Source`/`Plus` and throws for the
  rest. **No live call site needs more**: of the 7 retained-range calls, 6 are `Source`
  and 1 is `Plus`. Latent gap, not a live one.
- **`drawPolygon` cannot express a fill rule.** Real Qt has
  `QPainter::drawPolygon(const QPolygonF &, Qt::FillRule)`; `PkPainter::drawPolygon`
  takes only the polygon and `PkDrawPolygonCommand` has no such field, so a
  `WindingFill` polygon is **not expressible from the Pk side at all**. Registered
  rather than removed: of the 14 retained-range `drawPolygon` call sites (13 at R-51,
  +1 added by R-52's lens-blur restore; **all 14 are single-argument**) — zero live
  usage of the missing parameter (R-line rule: at zero
  measured usage, keep it and register, do not delete). The oracle's `Case::fillRule`
  field is therefore 0 for every case in the shape oracle's set, and the Qt side's
  winding dispatch is unreachable there — noted in `oracle/shape_primitive_cases.h` so
  nobody mistakes it for an oversight. (The R-52 blur oracle *does* exercise
  `Qt::WindingFill`, on the lens-blur iris polygon; it is equivalent there only because
  that polygon is always convex — measured in R-52 plan §2.1 P2.)
- **`PkSvgPainterBackend`** (`libs/flake/svg/`, the SVG-export backend) marks any
  command it does not model as unsupported, which invalidates the whole document and
  falls back to raster. Ellipse/arc/polygon are not modelled there either.
- **Two `[GAP]` regressions in `plugins/filters/blur/`** were not "unimplemented" but
  **deleted**: `kis_motion_blur_filter.cpp` and `kis_lens_blur_filter.cpp` used to build
  their convolution kernel by filling a polygon into a `PkImage` and reading the pixels
  back; that was replaced with a unit kernel. `KisConvolutionPainter` anchors a kernel at
  `((w-1)/2, (h-1)/2)` (`libs/image/kis_convolution_worker_spatial.h:57-58`), while the
  unit `1.0` sits at `(h/2, w/2)`; the two coincide exactly when both kernel dimensions
  are odd. **Motion kernels are odd by construction** (`kernelHalfSize*2 + (1,1)`), so
  motion blur stayed byte-identical — a true no-op — for every motion configuration,
  while lens kernels come from a bounding box and may be even, so lens blur became a
  **shift** (measured; see R-52 plan §1.1). Restoring them needed polygon fill +
  readback. **R-52 restores both** (see the R-52 section below).

**Host portability of this directory's scripts** (measured on macOS arm64, 2026-09-11):
`tests/run_tests.sh` and both `oracle/run_brush_*.sh` cannot run there —
they `source` a Linux path (`/mnt/ssd-disk/...`), and the two brush runners pass
`-Wl,--start-group`/`--end-group`, which GNU ld accepts but ld64 does not.
`tests/run_tests.sh` additionally calls `readelf`/`ldd` (Linux binutils; `otool -L` is
the macOS equivalent).
`oracle/run_shape_primitive.sh` rewrites `oracle/shape_primitive_golden.txt` in the
source tree on every run — that is deliberate (the golden is a checked-in record of the
Qt measurement, and regenerating it is how it stays honest), and it is idempotent: when
the two sides agree the file comes back byte-identical and `git status` stays clean. `oracle/run_shape_primitive.sh` is written to work on both and
locates the dependency env by searching upwards from `pk/render` rather than hardcoding
a path. The three older scripts are registered here as a gap, not fixed by R-51.

## Dependency checks

`tests/run_tests.sh` runs the brief's blanket `nm -u -C ... | grep -i qt` command and
reports its real exit code. That grep intentionally matches copied compatibility names
(`Qt::GlobalColor`, `Qt::AspectRatioMode`, and `pk_qt_assert`), so it cannot be a clean
linkage predicate without renaming measured public APIs. The runner therefore also
enforces a reviewed real-Qt class/C-ABI matcher with an explicit compatibility
allowlist, verifies the final test executable's `readelf`/`ldd` dependency closure, and
checks Ninja's complete `pkrender` command closure for Qt targets and libraries.

## R-52 — restoring the two blur-filter kernels (registered deviations)

R-52 closes the two `[GAP]` regressions registered under R-51 above:
`plugins/filters/blur/kis_motion_blur_filter.cpp` and `kis_lens_blur_filter.cpp` again
build their convolution kernel by rasterising a line / polygon into a `PkImage` and
reading the red channel back (they had been replaced by an identity kernel, making
motion blur a no-op and lens blur a constant shift). Evidence layers (R-52 plan §3.5):
the oracle (`oracle/run_blur_kernel.sh`) reproduces the two kernel constructions for
**138 cases** Qt-side and the Pk branch is byte-identical; the real filters, driven
end-to-end on a real `KisPaintDevice`, must (a) change the image and (b) match a
Qt-side golden kernel fed through `KisConvolutionPainter`; and `nm -u -C
libkritablurfilter.a` carries zero Qt-class symbols.

**Symbol criterion (R-52 plan §5 ③), measured on `/tmp/r52-build/lib/libkritablurfilter.a`:**

- numerator — `nm -u -C libkritablurfilter.a | grep -E '\bQ[A-Z][A-Za-z0-9_]*\b'` → **0**.
- denominator — 294 raw `nm -u -C` lines / **282** symbol lines (the rest are member
  headings) across **7** archive members.
- discriminating power — the same command on `bin/libkritaflake.dylib` (`otool -L`
  confirms Qt 5.15.7: QtSvg/QtXml/QtWidgets/QtGui/QtCore) → **39** hits.
- the old blanket `grep -i qt` is a **false-negative machine**: on that same dylib it
  finds only **6** of the 39, because most Qt class names (`QAction`, `QChar`, `QEvent`,
  `QMenu`, `QMetaMethod`, `QPixmap`, `QString`, …) do not contain the literal substring
  `qt`. It happens to read 0 on this archive, but it would also read 0 if `QImage` or
  `QString` symbols were present — hence the strong criterion above.

Registered deviations / gaps, each with its source and its measured numbers:

1. **`drawPolygon` cannot express `Qt::WindingFill`** (the general gap is registered
   under R-51 above). Upstream lens blur calls `drawPolygon(iris, Qt::WindingFill)`;
   `PkPainter::drawPolygon` has no fill-rule parameter, so the `PkPainterPath` default
   rule carries it. **Measured equivalent** (probe A, R-52 plan §2.1 P2): `FILLRULE
   cases=96 mismatches=0`, because the iris polygon is a regular polygon under an affine
   map (`rotate` + positive `scale`) and is therefore always convex — `CONVEX cases=24
   nonConvex=0`. Source: R-52 plan §6.1.

2. **`qRed()` has no Pk equivalent** — inlined as `(pixel >> 16) & 0xffu`. Source:
   `R线-spec`「R-15 遗留缺口」第 3 条 (assigned to S-04 or the first task that needs it);
   R-52 does not claim it.

3. **The target format is not upstream's `Format_RGB32`.** This repo's
   `PkImageRasterBackend::fillPath` (`libs/flake/PkImageRasterBackend.cpp:824-826`)
   accepts only `Format_ARGB32` / `Format_ARGB32_Premultiplied` / `Format_Grayscale8`;
   `PkImage::Format_RGB32` throws. R-52 uses `Format_ARGB32 + fill(0xff000000)`, which
   is **byte-identical over all 138 cases** (probe B, §2.2) — a different expression,
   not a behavioural deviation. **The trap to register**: `fill(0)` differs from real Qt
   in **126 of the 138 cases** (probe B `argb32_f0 mismatch=126`), because AA coverage
   written into ARGB32 un-premultiplies back to 255 in the red channel. Source: R-52
   plan §6.3.

4. **Large kernel sizes fail differently (known deviation — flagged for review).**
   `PkImage(width, height, format)` (`pk/image/PkImage.cpp:355`) throws `std::bad_alloc`
   when the pixel allocation fails (the `resize` at `:367`), whereas `QImage` degrades to
   a null image, its canvas operations become silent no-ops and the kernel stays
   all-zero. Trigger: `blurLength` / `irisRadius` driven to extreme values by user data
   (`.kra`, adjustment-layer configuration). R-52 deliberately adds no guard that
   upstream does not have (a guard would be new behaviour); upstream is itself
   integer-overflow UB on the same input. Source: R-52 plan §6.4.

5. **The `kritablurfilter` lock is narrower than this batch's landing site.**
   Batch 5 edits `plugins/filters/blur/`, which is **not** in R-52's `locks`
   (`.exec/tasks.yaml:213` = `[pk/render, libs/flake/PkImageRasterBackend.cpp]`); the
   main session is to correct the lock line. Source: R-52 plan §1.4 / §6.5.

6. **Pre-existing breakage (not an R-52 regression; registered, unclaimed).**
   `plugins/filters/tests/kis_all_filter_test.cpp` and `kis_crash_filter_test.cpp` do
   **not compile**: they reference `PkImage(fileName)`, `PkImage::save()` and
   `PkFileStream`, none of which exist in this repo (`PkFileStream` has no definition
   anywhere; `pk/` only has `pk/textstream/PkTextStream.h`). Neither file is in
   `.exec/baseline/tests.txt`. Raw symptom, this tree:

   ```
   plugins/filters/tests/kis_all_filter_test.cpp:28:13: error: no matching constructor for initialization of 'PkImage'
      28 |     PkImage qimage(PkString(FILES_DATA_DIR) + '/' + "carrot.png");
   plugins/filters/tests/kis_all_filter_test.cpp:41:5: error: unknown type name 'PkFileStream'; did you mean 'PkStream'?
      41 |     PkFileStream file(PkString(FILES_DATA_DIR) + '/' + f->id() + ".cfg");
   ```

   `kis_crash_filter_test.cpp:25/34` fail identically. Repair needs the image file codec
   closure (`R线-spec`「R-15 遗留缺口」第 1 条) plus a `PkFileStream` — M5 or a later S/R
   task. The upstream fixture PNGs exist (`plugins/filters/tests/data/{carrot.png,
   carrot_motion blur.png, carrot_lens blur.png}`), but the runner cannot be built,
   which is why R-52's end-to-end evidence is an in-memory driver instead of that test.
   Source: R-52 plan §6.6 / §7.

## R-55 — text (`setFont` / `drawText`) oracle

`oracle/text_cases.h` + `oracle/text_oracle.cpp` + `oracle/run_text.sh` +
`oracle/text_golden.txt` + `tests/test_text.cpp` are the text counterpart of the R-51
shape oracle. Two binaries, one shared case table: the Qt side is hand-compiled by the
runner with `-DPK_TEXT_QT_ORACLE` and calls `QPainter::setFont/drawText` directly (the
same mapping as `libs/flake/tests/PkQPainterAdapter.cpp:117-125`); the Pk side is the
CMake target `text_oracle_pk` going through `PkPainter` + `PkImageRasterBackend`. Case
shapes copy the two real call sites (`KoSvgTextShape_p_output.cpp:686`,
`SvgTextCursor.cpp:880`), plus adversarial cases and three explicit `setFont` cases
(neither real call site calls `setFont`). This is the driver-degradation path of
`R线-spec`「依赖墙挡住真实测试类时」; **the compile-level evidence for the real test
classes is still owed**.

**Engine-conditional, like `pk/font/oracle/compare.cmake`.** `run_text.sh` counts
`QFontEngineFT` in the QtGui it would link before comparing. On this machine that is
**0 of 9594 `nm` lines** (with `QFontEngineMulti=40` as the discriminating-power
control), so the runner prints `SKIP: Qt has no FreeType text engine (0)`, does **not**
run the Qt side, and writes the golden from the **Pk** side. The golden's birth
certificate therefore reads `# born=Pk` — it is **not** a Qt golden (plan §6 item 6).
On this machine `tests/test_text.cpp` is a regression guard over Pk's own output
(`44 cases (41 discriminating, 3 degenerate/no-op)`), **not** a claim of pixel equality
with Qt. On a FreeType host the runner rewrites the golden from the Qt side and the same
test becomes a real cross-side assertion.

**Adjudicated and fixed: the point overload's character-visibility rule.**
Task 4 discovered that Qt's `drawText(QPointF, QString)` discards `\n` completely (no
line break, no advance, no replacement glyph) while Pk gave it an advance of about 4 px.
That divergence is **fixed** in `PkImageRasterBackend::drawText`, and the rule turned out
to be reducible, not a special case for `\n`:

- **Criterion (first-hand Qt source, Qt 5.15.7-lts-lgpl).** `QTextEngine::applyVisibilityRules`
  (`qtbase/src/corelib/text/qtextengine.cpp:1361`) sets `QGlyphAttributes::dontPrint` on
  **U+000A `LineFeed`, U+000C `FormFeed`, U+000D `CarriageReturn`, U+2028 `LineSeparator`,
  U+2029 `ParagraphSeparator`, U+00AD `SoftHyphen`**. `dontPrint` zeroes the advance
  (`:1606 si.width += glyphs.advances[i] * !glyphs.attributes[i].dontPrint;`) and makes the
  raster engine skip the glyph. It is called from the live HarfBuzz path
  (`shapeTextWithHarfbuzzNG`, `:1759`); `qt_useHarfbuzzNG()` defaults to true
  (`qfontengine.cpp:97-101`). The rule sits in the **text engine, above the font engine**,
  so it holds on every platform and for every font.
- **Why only three of the six diverged.** U+2028/U+2029/U+00AD are `Default_Ignorable`, so
  HarfBuzz already hides them in Pk. LF/FF/CR are not, so Pk advanced the pen for them.
  `PkImageRasterBackend::drawText` now filters the whole six-codepoint set before calling
  `PkFontRasterizer::coverage()`. **`pk/font` is untouched** — this is the point overload's
  behaviour, not the glyph rasteriser's, and `render()`/`outline()` must stay byte-identical
  (`KisTextBrush` and SVG `<path>` text both consume them).
- **Sweep, not a probe.** The rule was measured over U+0000..001F, U+007F..009F, U+00A0,
  U+00AD, U+061C, U+180E, U+2000..200F, U+2028..202F, U+205F..2064, U+2066..206F, U+FEFF,
  U+FFF9..FFFB, U+110BD, comparing each character's advance *and* ink on both sides. Full
  table: `.superpowers/sdd/R-55/task-4fix-report.md` §2.
- **Witnesses in the table.** `adv/newline-mid` ("X\nY") and `adv/newline-twin` ("XY") now
  have the **same** digest, as do `vis/hidden/lf` and `vis/hidden/lf-twin`; before the fix
  they differed. The new `vis/hidden/*` cases (one per hidden category) and `vis/kept/*`
  cases (TAB, VT, space, NBSP, U+3000, U+007F, U+180E) are reverse guards: widening the
  filter set turns them red.

**Still open, registered — same layer, different implementation.** Not hidden by the rule
above and **not** fixed here; `run_text.sh` will legitimately go red on these on a FreeType
host:
- **TAB** (`vis/kept/tab`). Qt's point overload still runs the text engine's tab stop
  (`qtextengine.cpp:1341 calculateTabWidth`): measured 80.000 for `\t` alone and 94.656 for
  `"X\tY"` at DejaVu Sans 24px, versus Pk passing `U+0009` to Raqm (7.000).
- **U+2066/U+2067/U+2068/U+2069 and U+061C** (`vis/bidi/lri`, `vis/bidi/alm`). Hidden on
  both sides as single characters, but in a string Qt's output is bit-identical to the
  control character's absence while Pk's Raqm bidi pass moves the neighbouring glyphs.
- **U+007F, U+180E, U+FFF9..FFFB, U+0008, U+001D** (`vis/kept/del`, `vis/kept/mvs`). Hidden
  by Qt's **CoreText** font engine on this machine, not by Qt's text engine; Pk's FreeType
  path differs. This is the already-registered〈文字类对拍〉platform difference (plan §6
  item 1), so it is out of scope by construction.
