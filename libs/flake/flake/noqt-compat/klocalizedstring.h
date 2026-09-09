#pragma once

// Shapemodel undo labels are native KUndo2MagicString values. The only
// shapemodel source that includes this legacy KDE header uses kundo2_text(),
// which is provided by kundo2magicstring.h through kundo2command.h; no KDE
// localization declaration is required in this no-Qt compile closure.
