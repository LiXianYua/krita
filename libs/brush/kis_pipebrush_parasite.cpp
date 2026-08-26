/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2005 Bart Coppens <kde@bartcoppens.be>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "kis_pipebrush_parasite.h"

#include <KisPortingUtils.h>

KisPipeBrushParasite::KisPipeBrushParasite(const PkString& source)
{
    init();
    needsMovement = false;

    // 原 Qt: source.split(QLatin1Char(' '), Qt::SkipEmptyParts)。PkString::split(char16_t)
    // 不跳空段，但 parasite 串是单空格分隔、无连续分隔符，行为等价；畸形输入的真空段
    // 会被下方 else 链忽略，优雅降级（不为 skip-empty 语义额外加过滤）。
    const std::vector<PkString> parasites = source.split(u' ');

    for (int i = 0; i < static_cast<int>(parasites.size()); i++) {
        const std::vector<PkString> split = parasites.at(i).split(u':');

        if (static_cast<int>(split.size()) != 2) {
            warnImage << "Wrong count for this parasite key/value:" << parasites.at(i);
            continue;
        }
        const PkString &index = split.at(0);
        if (index == "dim") {
            dim = (split.at(1)).toInt();
            if (dim < 1 || dim > MaxDim) {
                dim = 1;
            }
        } else if (index.startsWith("sel")) {
            int selIndex = index.mid(3).toInt();

            if (selIndex >= 0 && selIndex < dim) {
                selectionMode = split.at(1);

                if (selectionMode == "incremental") {
                    selection[selIndex] = KisParasite::Incremental;
                } else if (selectionMode == "angular") {
                    selection[selIndex] = KisParasite::Angular;
                    needsMovement = true;
                } else if (selectionMode == "random") {
                    selection[selIndex] = KisParasite::Random;
                } else if (selectionMode == "pressure") {
                    selection[selIndex] = KisParasite::Pressure;
                } else if (selectionMode == "xtilt") {
                    selection[selIndex] = KisParasite::TiltX;
                } else if (selectionMode == "ytilt") {
                    selection[selIndex] = KisParasite::TiltY;
                } else if (selectionMode == "velocity") {
                    selection[selIndex] = KisParasite::Velocity;
                } else {
                    selection[selIndex] = KisParasite::Constant;
                }
            }
            else {
                warnImage << "Sel: wrong index: " << selIndex << "(dim = " << dim << ")";
            }
        } else if (index.startsWith("rank")) {
            int rankIndex = index.mid(4).toInt();
            if (rankIndex < 0 || rankIndex > dim) {
                warnImage << "Rankindex out of range: " << rankIndex;
                continue;
            }
            rank[rankIndex] = (split.at(1)).toInt();
        } else if (index == "ncells") {
            ncells = (split.at(1)).toInt();
            if (ncells < 1) {
                warnImage << "ncells out of range: " << ncells;
                ncells = 1;
            }
        }
    }

    for (int i = 0; i < dim; i++) {
        index[i] = 0;
    }

    setBrushesCount();
}

void KisPipeBrushParasite::init()
{
    for (int i = 0; i < MaxDim; i++) {
        rank[i] = index[i] = brushesCount[i] = 0;
        selection[i] = KisParasite::Constant;
    }
}

void KisPipeBrushParasite::sanitize()
{
    for (int i = 0; i < dim; i++) {
        // In the 2 listed cases, we'd divide by 0!
        if (rank[i] == 0 &&
                (selection[i] == KisParasite::Incremental
                 || selection[i] == KisParasite::Angular)) {

            warnImage << "PIPE brush has a wrong rank for its selection mode!";
            selection[i] = KisParasite::Constant;
        }
    }
}

void KisPipeBrushParasite::setBrushesCount()
{
    // I assume ncells is correct. If it isn't, complain to the parasite header.
    if (rank[0] != 0) {
        brushesCount[0] = ncells / rank[0];
    }
    else {
        brushesCount[0] = ncells;
    }

    for (int i = 1; i < dim; i++) {
        if (rank[i] == 0) {
            brushesCount[i] = brushesCount[i - 1];
        }
        else {
            brushesCount[i] = brushesCount[i - 1] / rank[i];
        }
    }
}

bool KisPipeBrushParasite::saveToDevice(PkStream* dev) const
{
    // write out something like
    // <count> ncells:<count> dim:<dim> rank0:<rank0> sel0:<sel0> <...>

    PkTextStream stream(dev);
    KisPortingUtils::setUtf8OnStream(stream);

    // XXX: FIXME things like step, placement and so are not added (nor loaded, as a matter of fact)"
    stream << ncells << " ncells:" << ncells << " dim:" << dim;

    for (int i = 0; i < dim; i++) {
        stream << " rank" << i << ":" << rank[i] << " sel" << i << ":";
        switch (selection[i]) {
        case KisParasite::Constant:
            stream << "constant"; break;
        case KisParasite::Incremental:
            stream << "incremental"; break;
        case KisParasite::Angular:
            stream << "angular"; break;
        case KisParasite::Velocity:
            stream << "velocity"; break;
        case KisParasite::Random:
            stream << "random"; break;
        case KisParasite::Pressure:
            stream << "pressure"; break;
        case KisParasite::TiltX:
            stream << "xtilt"; break;
        case KisParasite::TiltY:
            stream << "ytilt"; break;
        }
    }

    return true;
}

bool loadFromDevice(PkStream */*dev*/)
{
    // XXX: implement...
    return true;
}
