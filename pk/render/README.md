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
  rather than removed: of the 13 retained-range `drawPolygon` call sites, **all 13 are
  single-argument** — zero live usage of the missing parameter (R-line rule: at zero
  measured usage, keep it and register, do not delete). The oracle's `Case::fillRule`
  field is therefore permanently 0, and the Qt side's winding dispatch is unreachable —
  noted in `oracle/shape_primitive_cases.h` so nobody mistakes it for an oversight.
- **`PkSvgPainterBackend`** (`libs/flake/svg/`, the SVG-export backend) marks any
  command it does not model as unsupported, which invalidates the whole document and
  falls back to raster. Ellipse/arc/polygon are not modelled there either.
- **Two `[GAP]` regressions in `plugins/filters/blur/`** are not "unimplemented" but
  **deleted**: `kis_motion_blur_filter.cpp` and `kis_lens_blur_filter.cpp` used to build
  their convolution kernel by filling a polygon into a `PkImage` and reading the pixels
  back; that was replaced with an identity kernel, so **both filters are currently
  no-ops**. Restoring them needs polygon fill + readback, i.e. the path this change adds.

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
