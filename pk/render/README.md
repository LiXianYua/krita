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

`pk/render/PkPaintCommand.h` defines **27** command types. `PkImageRasterBackend::submit`
(`libs/flake/PkImageRasterBackend.cpp`) explicitly handles **all 27** — 24 via
`std::get_if<...>` branches (`:448`-`:624`) plus `PkSaveCommand` / `PkRestoreCommand` /
`PkDrawTextInRectCommand` via `std::holds_alternative<...>` (`:574` / `:578` / `:624`). Of
those, **26 do work and 1 throws** unconditionally (`PkDrawTextInRectCommand`, the explicit
throw-only branch at `:624-631`); the generic throw at `:636` is a defensive fallback no
known command reaches. **Counts re-measured live on this tree, 2026-09-12, R-54 Task 3**
(the sentence it replaces read "dispatches 26 of them; **25 do work and 2 throw**", which was
internally inconsistent — 25+2=27≠26). R-54 batch 1b is what moved `PkDrawArcCommand` out of
the throw set; R-55 Task 3 had implemented `drawText`@point + `setFont`.

The table below is the **diff set** R-51 measured, with R-55 Task 5's re-measured
call-site counts. Every count is call sites verified by receiver type (not by grep), over
the retained range (`libs/ plugins/ pk/ sdk/`, excluding `tests/`, `benchmarks/` and
`oracle/` at any depth).

| Command | Raster backend | Live call sites | Files | Batch |
|---|---|---|---|---|
| `PkDrawPolygonCommand` | **implemented 2026-09-11** (`addPolygon` + `closeSubpath`) | **14** | **5** | R-51 |
| `PkDrawEllipseCommand` | **implemented 2026-09-11** (`addEllipse`) | 10 | 6 | R-51 |
| `PkDrawArcCommand` | **implemented 2026-09-12** (`normalized()` + `arcMoveTo`/`arcTo`) | 2 | 1 | R-54/1b |
| `PkDrawTextAtPointCommand` | **implemented 2026-09-12** (`PkImageRasterBackend::drawText`) | **2** | **2** | R-55/3 |
| `PkDrawTextInRectCommand` | throws (registered deviation — see the R-55 Task 5 table below) | **0** | **0** | R-55/5 |
| `PkSetFontCommand` | **implemented 2026-09-12** | 0 | 0 | R-55/3 |
| `PkSvgPainterBackend` (SVG-export backend, `libs/flake/svg/`) | **models 21 of 27 commands**; R-54 batch 2 added the three draw primitives (ellipse / arc / polygon); 6 still fall to `m_supported = false` | 1 | 1 | R-54/2 |

Two rows were corrected by R-55 Task 5 and the correction matters: R-51 recorded
`PkDrawTextAtPointCommand` as 1 site / 1 file and `PkDrawTextInRectCommand` as **1 site /
1 file**, i.e. it claimed the `PkRectF` overload had a live caller. It does not.
Re-measured on this tree with the retained-range command
`grep -rn --include='*.cpp' --include='*.h' --include='*.cc' --include='*.cxx'
'\\bdrawText\\s*(' libs plugins pk sdk` (minus `tests/`, `benchmarks/`, `oracle/`), the
**7** occurrences outside `pk/render` itself are exactly:

```
libs/flake/PkImageRasterBackend.h:20      declaration
libs/flake/PkImageRasterBackend.cpp:609   definition (submit dispatch)
libs/flake/PkImageRasterBackend.cpp:1166  comment
libs/flake/PkImageRasterBackend.cpp:1247  definition
libs/flake/text/KoSvgTextShape_p_output.cpp:686    ← real call site ①  (PkPointF, PkString)
libs/image/kis_threaded_text_rendering_workaround.h:11   comment
plugins/tools/svgtexttool/SvgTextCursor.cpp:880    ← real call site ②  (PkPointF, PkString)
```

Both live call sites pass a **point**, so the `PkRectF` overload's live-call-site count is
**0**, not 1 — which is the numeric basis for the zero-usage registration of
`PkDrawTextInRectCommand`. `PkSetFontCommand`'s 0 is re-confirmed the same way:
`libs/brush/kis_text_brush_factory.cpp:35` `brush->setFont(font)` has receiver
`KisTextBrush`, not `PkPainter` (`KisTextBrush` rasterises through
`PkFontRasterizer::render()/outline()` directly, never through `PkPainter`), so it is not
a `PkSetFontCommand` call site. There is no `PkPainter::setFont` call in the retained
range at all.

`drawPolygon`, `drawEllipse` and (since R-54 batch 1b) `drawArc` are Qt 5.15.7-pixel-identical
to building the corresponding `PkPainterPath` and routing it through the existing fill/stroke
machinery — verified case-by-case by `oracle/run_shape_primitive.sh`. **Re-measured live on
this tree, 2026-09-12 (R-54 Task 3): 288 cases, of which 161 are discriminating and 127 are
degenerate/no-op** (the R-51 figure this replaces was "36 cases, 29 discriminating, 7
degenerate"; R-54 batch 1b added the arc family, which is most of the no-op growth — a
`pen==0, brush==1` arc draws nothing on both sides because arcs ignore the brush). The
degenerate cases are kept as "degenerate input draws nothing, on both sides" assertions; they
are not independent evidence of drawing correctness, and the test binary prints the split
(`288 cases (161 discriminating, 127 degenerate/no-op)`) so the number cannot be misread.

The one empirical trap: `QPainter::drawPolygon` **closes the subpath** before
stroking; `PkPainterPath::addPolygon` produces an *open* one. Omitting `closeSubpath()`
turns **6 of the 288 cases red** (re-measured live, R-54 Task 3 — the six 4-vertex /
self-intersecting / explicitly-closed polygon stroke cases
`polygon#22/23/28/29/31/32`); R-51's per-case pixel cost for those quads was 74 / 76 / 106
pixels (32x32 ARGB32, per-pixel packed-value compare) — **not re-derived here**, because the
shape oracle compares per-image digests, not pixel counts.

**Registered gaps, with their blockers:**

- **`drawArc` — ~~blocked on `pk/geometry`~~ — closed by R-54 (batch 1b).** The blocker
  was that `PkPainterPath::arcMoveTo` was `private:` (real Qt exposes
  `QPainterPath::arcMoveTo` as public API), so `arcTo` on an empty path started from
  `(0, 0)`, unlike Qt. R-54 batch 1b made `arcMoveTo` public in `pk/geometry` and gave
  `PkImageRasterBackend::submit` an arc branch (`libs/flake/PkImageRasterBackend.cpp:499`).
  Semantics and the `normalized()` deviation are in the R-54 section below.
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
  command it does not model as unsupported, which invalidates the whole document
  (`document()` returns `{}`) and makes the caller fall back to raster. **R-54 batch 2
  models three primitives there** — ellipse / arc / polygon, semantics aligned
  command-for-command with the raster backend — so it now handles **21 of 27**
  commands; **6 still fall to `m_supported = false`**: `PkFillTexturePathCommand`,
  `PkDrawPointCommand`, `PkDrawPixmapCommand`, `PkDrawTiledPixmapCommand`,
  `PkDrawTextAtPointCommand`, `PkDrawTextInRectCommand`. See the R-54 section below.
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
- **Modelling difference, registered.** Qt keeps the suppressed glyph in the shaping
  buffer and only keeps its advance out of the *following* glyph's position
  (`qtextengine.cpp:1606`); this implementation removes the character before shaping.
  Equivalent on every measured sample, and could differ only where a hidden character
  participates in its neighbours' shaping (kerning across it, Arabic joining, ligature
  formation) — neither live call site can produce that. Registered in full as item **C10**
  in `pk/font/README.md`, together with the rest of the R-55 deviation table.

**Still open, registered — same layer, different implementation.** Not hidden by the rule
above and **not** fixed here; `run_text.sh` will legitimately go red on these on a FreeType
host. Each carries its measured numbers and its source; the full sweep table is
`.superpowers/sdd/R-55/task-4fix-report.md` §3:

- **TAB (U+0009)** — `vis/kept/tab`. Qt's point overload still runs the text engine's tab
  stop (`qtextengine.cpp:1341 calculateTabWidth`), i.e. **the same layer** as
  `applyVisibilityRules`, not a font-engine behaviour. Measured at DejaVu Sans px24:
  `U+0009` alone Qt advance **80.000** vs Pk **7.000**; `"X\tY"` Qt **94.656** vs Pk
  **38.000**. Qt's 80.000 is `QTextOption::tabStop`'s default, not a font metric; Pk hands
  `U+0009` to Raqm as an ordinary glyph. Source: task-4fix-report.md §6.1.
  **Why keep and register rather than fix:** both real call sites' text contains no tab
  (`KoSvgTextShape_p_output.cpp` labels are `#<n>~<m>\n(<idx>)`, `SvgTextCursor.cpp` names
  come from a fixed `i18nc` handle table) — zero measured usage. Fixing it would require
  splitting `coverage()`'s "one string → one mask" exit into per-segment placement, a
  design change outside this batch.
- **U+2066 / U+2067 / U+2068 / U+2069 / U+061C** — `vis/bidi/lri`, `vis/bidi/rli`,
  `vis/bidi/fsi`, `vis/bidi/pdi`, `vis/bidi/alm`. Hidden on both sides as single
  characters, but inside a string Qt's output is bit-identical to the control character's
  absence while Pk's Raqm bidi pass moves the neighbouring glyphs. Measured: Pk digest for
  `"AB"` = **2598421920**; `A LRI B` / `A RLI B` / `A LRI B PDI` / `A FSI B` all =
  **2038677757**; `A RLO B` / `A PDF B` **equal** `"AB"`. Qt side: `A LRI B`'s advance and
  ink are pixel-identical to `"AB"` (260 non-white pixels, bbox `[20..50]`). Source:
  task-4fix-report.md §6.2. **Why keep and register:** this is not a "dropped character"
  defect — the advance is 0 on both sides and neither emits ink; it is Raqm's bidi
  resolution versus Qt's own (`QTextEngine::itemize`), a different implementation on a
  different layer, and belongs to another line. All five codepoints are zero-usage in the
  two real call sites.
- **U+007F, U+180E, U+FFF9..FFFB, U+0008, U+001D** — `vis/kept/del`, `vis/kept/mvs`.
  Hidden by Qt's **font engine** (CoreText on this machine), not by Qt's text engine: the
  five-font consistency column of the sweep reads `no` for all five (U+007F dropped by
  3/5 fonts, U+0008 and U+001D by 4/5, U+FFF9/U+FFFA/U+FFFB by 1/5 — DejaVu only), which is
  exactly the discriminator between "text-engine rule" (5/5) and "font-engine behaviour"
  (mixed). Pk's FreeType path differs. This is the already-registered〈文字类对拍〉platform
  difference (plan §6 item 1) — **out of scope by construction**, not by omission. Source:
  task-4fix-report.md §2.1 / §6.3.
  **The temptation to filter them too must be resisted**: doing so would contradict the
  standing "align to Qt-**with-FreeType**" ruling (on a FreeType host Qt goes through
  FreeType and may well *not* hide them). `vis/kept/del` and `vis/kept/mvs` exist as the
  reverse guards that turn red if anyone widens the filter set.

### R-55 Task 5 — closing registration (判据② 降级路径 + plan §6 清单)

**1. The driver artifact `text_draw_driver` (判据② 的依赖墙降级路径).**
`tests/graft/text_draw_driver.cpp` + `tests/graft/stubs/kritaflake_export.h` are a
CMake target (no `add_test` — it is 判据②'s trial-link **evidence**, not a criterion of its
own; same shape as the R-16 precedent `pk/time/tests/graft/date_parser_driver.cpp`). It
transcribes, line for line, the code shape of the two real `drawText` call sites
(`libs/flake/text/KoSvgText_shape_p_output.cpp:686`,
`plugins/tools/svgtexttool/SvgTextCursor.cpp:880`), and it **self-annotates as a
substitute** in its header comment and in four `DRIVER-NOTICE` stdout lines. Registration
reasons, per `R线-spec`「依赖墙挡住真实测试类时」:

- **Which wall, and why it is outside the locks.** File ① compiles into target
  `kritaflake` (`libs/flake/CMakeLists.txt:563`, source listed at `:287`/`:494`); file ②
  into `krita_tool_svgtext_static`
  (`plugins/tools/svgtexttool/CMakeLists.txt:49`). Their **CMake-generated products**
  (`kritaflake_export.h` via `generate_export_header` @ `libs/flake/CMakeLists.txt:574`,
  `kritatoolsvgtext_export.h` @ `plugins/tools/svgtexttool/CMakeLists.txt:51`, the ECM
  macros) and their `PUBLIC` closure (`kritaimage`/`kritaui`) are outside R-55's locks
  (`pk/render`, `pk/font`, `libs/flake/PkImageRasterBackend.cpp`, `libs/flake/text`,
  `plugins/tools/svgtexttool`). **Holding the `libs/flake/text` and
  `plugins/tools/svgtexttool` locks means "allowed to edit those directories", not "can
  build those targets"** — building them needs their own generated headers plus the whole
  closure. Verified first-hand with a layered `-fsyntax-only` probe, raw errors in
  `task-5-report.md` §2.
- **Validation values from real-Qt probes: cannot be given on this machine, and it says
  so.** This host's Qt has **no FreeType text engine** (`QFontEngineFT=0 of
  nm_lines=9594`, probed independently by `pk/font/oracle/compare.cmake` and
  `run_text.sh`), so the text-rasterisation reference frame does not exist here. The driver
  therefore carries **no "cross-checked against real Qt" claim**; every number it prints is
  **Pk-side self-produced** and must not be read as a Qt golden (〈文字类对拍〉ruling
  2026-09-12; plan §6 items 1 and 6). What it does verify is (a) same-engine cross-check
  against the oracle case table (`callsite1/glyphdebug/n{0,1,2}` digests equal
  `oracle/text_golden.txt`) and (b) invariants internal to the transcribed code.
- **The stub is a compile parameter, not an edit.** `tests/graft/stubs/kritaflake_export.h`
  substitutes CMake's generated export header purely so the driver can compile the **real**
  `libs/flake/kis_painting_tweaks.cpp` (whose `luminosityCoarse()` computes call site ②'s
  pen colour). `libs/flake/kis_painting_tweaks.cpp` is not copied and is not modified;
  the stub lives only on this driver's `-I` path and enters no library — so 判据③'s `nm`
  surface is unaffected.

**2. `PkDrawTextInRectCommand` — zero usage, kept and registered** (plan §6 item 2; row in
the coverage table above corrected to **0 live call sites / 0 files**). The command carries
only `{ PkRectF rect; PkString text; }`, so it **cannot express** Qt's
`drawText(QRectF, int flags, QString, QRectF *)` overload — there is no flags field — and
`PkImageRasterBackend::submit` throws for it (throw-only branch at
`libs/flake/PkImageRasterBackend.cpp:612-618`, with the reason in a comment). Same class of
decision as R-51's fill-rule-less `drawPolygon`: at zero measured usage the R-line rule is
**keep and register, do not delete**.

**3. Two findings made while writing the driver** (both are *measurement* results, neither
is a behaviour change):

- **`bgColorForCaret()` can only return black or white — the case table's comment about the
  call-site-② pen colour is wrong.** `SvgTextCursor.cpp:786-789` is
  `luminosityCoarse(c) > 0.8 ? PkColor(0,0,0,opacity) : PkColor(255,255,255,opacity)`, a
  two-way ternary. For the case table's `selectionColor = 0x2a6fd6` it returns
  **`(255,255,255,255)` = white** — measured, asserted in the driver — not the grey the
  table's comment claims. Consequence, measured: a *faithful* transcription draws white on
  the oracle's white canvas and produces **exactly the blank digest** (Pk `digest ==
  1332944325` for the empty image), which is precisely why `oracle/text_cases.h` needed a
  visible stand-in colour. `oracle/text_cases.h:286` hard-codes
  `c.penRgb = 0x3f3f3f` with the comment "`bgColorForCaret(selectionColor,255)` 的灰" —
  **the value is a legitimate parametric choice, the comment is false**. The driver pins the
  truth (`bgColorForCaret(0x2a6fd6, 255).rgba() == 0xffffffff`) so the case table's case
  shapes are exercised with the real colour *and* with a visible one.
- **Call site ② single-maps its point (an earlier "double-maps" claim here was wrong).**
  `SvgTextCursor.cpp:795` sets `gc.setTransform(shape->absoluteTransformation(), true)`, but
  after `painterTf` is sampled at `:830` the code constructs a `KisHandlePainterHelper` at
  `:831`, whose `init()` **resets the painter transform to identity**
  (`libs/flake/KisHandlePainterHelper.cpp:59`; restored on destruction, `:71-75`). So at
  `:880` the painter transform is identity: the point passed is already
  `painterTf.map(closestBaselinePoint)` (device space) and `PkImageRasterBackend::drawText`'s
  `m_state.transform.map(command.position)` (`libs/flake/PkImageRasterBackend.cpp:1264`) is
  the **only** mapping — single-mapped. A prior revision of this note (and of the driver,
  `tests/graft/text_draw_driver.cpp`) wrongly claimed `:795`'s transform is never reset and
  the point is double-mapped; that model left the driver's painter at the shape transform
  instead of replicating the helper's reset, diverging from the real call site. Corrected in
  the 2026-09-12 fix round: the driver now performs the helper's `setTransform(identity)`
  step, and Task 4's case table (`callsite2/handlename/devpt30/*`) uses an identity painter
  transform plus the device point `(30,30)`.

**4. The two remaining plan §6 items owned by `pk/font`** — item 1 (判据④ unreachable
locally, engine-conditional, with the 几何类 discriminating-power control green here) and
item 6 (`text_golden.txt`'s `# born=Pk` birth certificate) — plus item 3 (empty-`PkFont`
resolution is platform-dependent) and item 4 (gamma(CFF) coloured-glyph composition
unverified) and item 5 (fontconfig config must be set explicitly) are registered in
**`pk/font/README.md`**, which holds the batch's complete deviation table. One line each
from the render side, so this file is readable on its own: **item 3** — Qt resolves an
empty `PkFont` to `.AppleSystemUIFont` 12pt, Pk to fontconfig `sans-serif` + 12pt@96dpi (a
scope decision, not an omission); **item 4** — the coloured-glyph (CBDT/COLR) composite
path has no test on either side; **item 5** — without `FONTCONFIG_PATH` pointing at the CI
prefix's `_install/etc/fonts` the same binary goes red for environment reasons (measured
once; hence the value is baked into the ctest environment in `CMakeLists.txt`).

## R-54 — shape primitives, batch 2: the SVG-export backend (plus batch 1b's raster `drawArc`)

R-54 lands two batches over the same oracle pattern R-51/R-52/R-55 built:

- **Batch 1b** — `PkDrawArcCommand` in the **raster** backend
  (`libs/flake/PkImageRasterBackend.cpp:499-511`). Qt's `drawArc(rect, a, span)` is
  `arcMoveTo(rect, a/16.0)` + `arcTo(rect, a/16.0, span/16.0)`, **stroke only** (the brush
  is ignored — measured), and Qt **normalises the rect inside `drawArc`**, so the branch uses
  `arc->rect.normalized()` (`arcMoveTo`/`arcTo` do *not* normalise themselves — R-54 §4b).
- **Batch 2** — `PkSvgPainterBackend` (`libs/flake/svg/PkSvgPainterBackend.h`), the
  SVG-export backend, gains the same three primitives (ellipse / polygon / arc), semantics
  aligned command-for-command with the raster backend. Any command it does not model still
  sets `m_supported = false`, and `document()` then returns `{}` — the caller's
  raster-fallback path.

**Oracle shape (判据④).** Batch 1b reuses R-51's `oracle/run_shape_primitive.sh` (per-pixel
FNV-1a over a 32×32 ARGB32 image; both sides render through the shared
`oracle/shape_primitive_cases.h`). Batch 2 **cannot** diff document text — the two sides' SVG
documents differ by definition (`<ellipse>` vs `<path>`, different attribute sets) — so
`oracle/run_svg_primitive.sh` renders **both** documents through the **same** `QSvgRenderer`
into a 32×32 ARGB32 image and compares pixels. `tests/test_svg_primitive.cpp` is a Qt-free
ctest guard that compares only the `doc=` column (the Pk document's FNV-1a); it proves "the
Pk document has not drifted", **not** equivalence with Qt — the two must be run on the same
commit, and the batch-2 CMake block says so at the target.

**Measured live on this tree, 2026-09-12 (R-54 Task 3; the `run_svg_primitive.sh` row below was
re-measured by **R-54 修复轮 1**), thin-shell build `/tmp/r54-pkrender-build`:**

| item | value |
|---|---|
| `run_shape_primitive.sh` | `identical (288 cases)` — 161 discriminating, 127 degenerate/no-op |
| `run_svg_primitive.sh` | `identical (288 cases)`; comparator `DIFF total=288 mismatch=0` |
| thin-shell ctest | **7 / 7 pass, 0 skipped** (`test_pksvg_rasterizer`, `test_shape_primitive`, `test_svg_primitive`, `test_blur_kernel`, `test_text`, `test_pkfont`, `test_pkxml`) |
| 判据③ `nm -u -C libpkrender.a \| grep -E '\bQ[A-Z][A-Za-z0-9_]*\b'` | **0 hits** (denominator: 12 objects, 16998 raw `nm` lines, 683 undefined symbols) |
| 判据③ same command on `svg_primitive_oracle_pk` / `svg_backend_driver` | **0 / 0** (6805 / 6883 raw lines; 163 / 166 undefined symbols) |
| discriminating control: same command on `krita/build-macos/bin/libkritaflake.dylib` (links Qt 5.15.7 — `otool -L`) | **39 hits** — the criterion has discriminating power |
| deprecated negative control: `nm -u libpkrender.a \| grep -i qt` | **1 hit** — a false positive on a Qt-free target (`pk_qt_assert`). This is why `grep -i qt` is retired. |

**判据② — both batches sit behind a dependency wall, so the R线-spec degrade path is used.**
Batch 1b's driver is `tests/test_shape_primitive.cpp` (R-51's product — R-54 Task 3 did **not**
rewrite it); its header self-declares as a driver replicating the real call-site shape. Batch
2's driver is `tests/graft/svg_backend_driver.cpp` (new in R-54 Task 3), shaped after the R-55
precedent `tests/graft/text_draw_driver.cpp`: a standalone executable, **no `add_test`**. Both
satisfy the four conditions of 〈依赖墙挡住真实测试类时〉:

- **(1) transcribe the real code shape.** Batch 2 replicates `libs/flake/svg/SvgWriter.cpp:242-254`
  — `PkSvgPainterBackend backend;` (the **default** ctor), `PkPainter painter(backend);`, the same
  pen/brush/transform order, `backend.document()` — and documents the real SVG consumption path
  (`SvgWriter.cpp:255-277`: non-empty → `addCompleteElement`, empty → raster fallback) in its
  header comment. Batch 1b's arc cases reach the backend through the shared `renderCase()`,
  whose arc branch is verbatim `painter.drawArc(c.shapeRect, c.startAngle16, c.spanAngle16)` —
  the same function, same parameter types / count / order as the two real call sites
  `plugins/tools/tool_knife/CutThroughShapeStrategy.cpp:371-372`.
- **(2) validation values from a real-Qt probe.** Batch 1b compares against
  `oracle/shape_primitive_golden.txt`, whose provenance header is `# qt=5.15.7` /
  `# backend=Qt5Gui`. Batch 2's driver is Qt-free and prints only **its own** document hashes,
  so its validation comes from a hand-compiled real-Qt probe; run over the driver's 276
  call-site-shaped cases the probe reports **`PROBE2 total=276 mismatch=0`** (probe command
  and raw output in `.superpowers/sdd/R-54/task-3-report.md` §3).
- **(3) explicitly a substitute.** Batch 2's driver header and four `DRIVER-NOTICE` stdout
  lines state that it is **not** `libs/flake/svg/SvgWriter.cpp`, and that its printed hashes
  are not a Qt-equivalence claim.
- **(4) name the wall.** Batch 2's real call site compiles into target **`kritaflake`** (same
  conclusion as R-55): **holding the `libs/flake/svg` lock means "allowed to edit that
  directory", not "can build the target"** — `kritaflake`'s CMake-generated products
  (`kritaflake_export.h`, the ECM macros) and its `PUBLIC` closure (`kritaimage`/`kritaui`)
  are outside R-54's locks. Batch 1b's call site compiles into `plugins/tools/tool_knife/`,
  likewise outside the locks.

**Closed — the batch-2 angle-coverage hole (was: measured, not fixed; closed by R-54 修复轮 1).**
Both batches' case tables originally had the same 判据④ coverage hole: every arc angle was a
multiple of 16, so `startAngle16 / 16.0` → `/ 16` was a compile-time no-op with zero
discriminating power over the whole arc family. **R-54 Task 3 Step 1b fixed it for batch 1b**
(`oracle/shape_primitive_cases.h` appends four `x % 16 != 0` angle groups; the falsification
`/16.0` → `/16` in `PkImageRasterBackend.cpp:509-510` turns the oracle **red** — 48 differing
cases, all at index ≥ 204, the 204 pre-existing cases untouched). **R-54 修复轮 1 closed the
identical hole in batch 2**: `oracle/svg_primitive_cases.h` appends four `x % 16 != 0` angle
groups (same append-after-the-existing-groups pattern), and the falsification `/16.0` → `/16`
in `PkSvgPainterBackend.h:215-216` now turns the batch-2 oracle **red**. Measured live on this
tree (2026-09-12, thin-shell build `/tmp/r54-pkrender-build`):

| step | command | reading |
|---|---|---|
| **before** (204-case table) + `PkSvgPainterBackend.h` mutated `/16.0`→`/16` | `run_svg_primitive.sh` | `identical (204 cases)`, **0** `DIFFTAG` lines — the no-op |
| after (288-case table), unmutated | `run_svg_primitive.sh` | `identical (288 cases)`, comparator `DIFF total=288 mismatch=0` |
| **after** (288-case table) + same mutation | `run_svg_primitive.sh` | `DIVERGED`, `DIFF total=288 mismatch=46`, rc=1 |

The 46 red cases are **entirely** in the four appended angle groups (per group: `{415,3615}`
12, `{-415,-2879}` 12, `{2879,911}` 12, `{0,1455}` 10) and span indices **205–287** — all
inside the appended region (`index ≥ 204`); **0** of the 204 pre-existing cases move, and the
pre-existing 204 lines of `oracle/svg_primitive_golden.txt` are byte-identical before and after
(verified by `diff`). The appended groups do not regress the green direction either: the
unmutated 288-case run is `mismatch=0` against the real-Qt comparator.

**Batch-2 driver input set (why 276 of 288).** The driver replicates the real call site's
**default** ctor, which emits an `<svg>` with **no** `width`/`height`/`viewBox`, whereas the
oracle uses the **bounds** ctor (`canvasRect()` = 32×32) for 1:1 pixel comparison. It keeps
all polygons and all arcs and drops the 12 ellipse cases on the four adversarial rects
(degenerate point / negative size / off-canvas / the flat `{2,2,1,28}`), which are 判据④
adversarial coverage inputs, not call-site shapes. Measured live (R-54 修复轮 1): the driver
prints `DRIVER-SUMMARY emitted=276 failures=0`, and the real-Qt probe over those **276**
call-site-shaped cases reports **`PROBE2 total=276 mismatch=0`**. With the default ctor over
**all 288** cases the probe reports **`PROBE2 total=288 mismatch=2`** — only `svg-ellipse#19`
and `#20` (the flat `{2,2,1,28}` rect), 4 px each: a no-`viewBox` document is **stretched by
its content bbox** to the render target (~8× here), amplifying Pk's four-cubic-Bézier ellipse
approximation into a 4-pixel edge difference. That stretch is why the driver set excludes
those ellipse rects — the reasoning is written out in `.superpowers/sdd/R-54/task-3-report.md`
§3.

**Still owed — the main-tree test `PkSvgPainterBackendTest.cpp` was not given the three primitives
(registered debt; lock boundary).** `libs/flake/flake/tests/PkSvgPainterBackendTest.cpp` is the
pre-existing main-tree test for `PkSvgPainterBackend` — 120 lines, 2 `private Q_SLOTS` functions
(`preservesVectorPathPainting()` and `preservesGradientAndImagePainting()`). Its comparison surface
is **the same one batch 2 built**: a Qt `QSvgGenerator` reference document beside the backend's own
`document()`, each rendered through `QSvgRenderer` and compared pixel-for-pixel (`:63-71`, `:109-116`
— `QSvgGenerator generator` … `PkSvgPainterBackend backend` … `QSvgRenderer expected(reference),
actual(native)`). Its purpose is to exercise the backend's primitives, so the three primitives batch
2 adds — `drawEllipse` / `drawPolygon` / `drawArc` — **belong** in it: measured live on this tree
(2026-09-12, `grep -c` over the file), each of the three occurs **0** times, while `fillPath`,
`drawLine`, `drawImage` and `fillRect` are present.

- **What is owed:** main-tree test cases for the three primitives.
- **Why it is owed:** the file is in `libs/flake/flake/tests/`, which is **not** in R-54's `locks`
  (`libs/flake/svg` covers only `svg/**`; `libs/flake/PkImageRasterBackend.cpp` covers only that one
  file). Per the R-52 §1.5 precedent this batch does **not** cross the boundary — crossing it would
  only make "is this one task or two" less clear.
- **Who pays, and can it be paid now:** a later task, or this one **after** the lock line is
  corrected — not from inside the current lock set.
- **Request to the main session: correct R-54's lock line** to include `libs/flake/flake/tests/`.
  **This is a request, not a self-correction — do not edit `.exec/` from the implementation side**
  (R-52 §1.4 disposition).

**Still open, registered — the compile-level 判据② evidence for the real call sites is still owed**
(same standing as R-51 §2 and R-55 §5). Batch 1b's real call site compiles into
`plugins/tools/tool_knife/`; batch 2's into target `kritaflake`. Both, together with their
CMake-generated products and `PUBLIC` closure, are **outside** R-54's `locks` and build closure —
**the same wall R-55 names**: holding the `libs/flake/svg` lock means "allowed to edit that
directory", not "can build the target". The driver-level evidence registered above
(`tests/test_shape_primitive.cpp`, `tests/graft/svg_backend_driver.cpp`) is **accepted as this
batch's completion criterion**; the compile-level evidence can be added once the blocking modules
have had their Qt stripped by later tasks. It is **not** required to be backfilled here.
