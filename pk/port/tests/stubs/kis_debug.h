#pragma once

// csv_read_line.cpp includes kis_debug.h but does not use its declarations.
// Keep the focused pk/port consumer test independent of the full Krita global
// target and its unrelated logging closure.
