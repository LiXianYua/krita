# pk/render

`PkPainter` is a Qt-free command producer. Every state mutation and draw submits one
typed `PkPaintCommand` to the mandatory `PkPainterBackend`; values are copied so
paths and images remain valid after their source values go out of scope. `save` and
`restore` track the complete local pen/brush/transform/render-hint state.

The measured surface is limited to Crop, Knife, Karbon Calligraphy, and Smart Patch:
state commands, line/rect/ellipse/arc/path/polygon/image primitives. Text, fonts,
pixmaps, gradients, textures, rasterization, and a Qt backend are intentionally absent.
