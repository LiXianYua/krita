#pragma once

#include "tga.h"

bool validateTgaHeader(const TgaHeader &header);
int tgaDestinationX(const TgaHeader &header, int sourceX);
bool validateTgaExportDimensions(int width, int height);
