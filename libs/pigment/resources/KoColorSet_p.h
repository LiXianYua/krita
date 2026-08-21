/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef KOCOLORSET_P_H
#define KOCOLORSET_P_H

#include <PkXmlStreamReader.h>
#include <PkXmlElement.h>
#include <PkXmlDocument.h>
#include <PkAuxTypes.h>
#include <kundo2stack.h>

#include <PkList.h>
#include <PkSet.h>

#include <KisSwatch.h>
#include <KisSwatchGroup.h>

#include "KoColorSet.h"

#include <memory>

class KoStore;          // 薄壳头不引 libs/store/KoStore.h，声明参数用到的类
class KoColorProfile;   // 同上，仅指针返回

struct RiffHeader {
    quint32 riff;
    quint32 size;
    quint32 signature;
    quint32 data;
    quint32 datasize;
    quint16 version;
    quint16 colorcount;
};

class KoColorSet::Private
{

public:
    Private(KoColorSet *a_colorSet);

public:
    KisSwatchGroupSP global() {
        Q_ASSERT(swatchGroups.size() > 0 && swatchGroups.first()->name() == GLOBAL_GROUP_NAME);
        return swatchGroups.first();
    }
public:
    bool init();

    bool saveGpl(PkStream *dev) const;
    bool loadGpl();

    bool loadAct();
    bool loadRiff();
    bool loadPsp();
    bool loadAco();
    bool loadXml();
    bool loadSbz();
    bool loadAse();
    bool loadAcb();
    bool loadCss();

    bool saveKpl(PkStream *dev) const;
    bool loadKpl();

public:

    KoColorSet *colorSet {0};
    KoColorSet::PaletteType paletteType {UNKNOWN};
    PkByteArray data;
    PkString comment;
    PkList<KisSwatchGroupSP> swatchGroups;
    KUndo2Stack undoStack;
    bool isLocked {false};
    int columns;

private:

    friend struct AddSwatchCommand;
    friend struct RemoveSwatchCommand;
    friend struct ChangeGroupNameCommand;
    friend struct AddGroupCommand;
    friend struct RemoveGroupCommand;
    friend struct ClearCommand;
    friend struct SetColumnCountCommand;
    friend struct SetCommentCommand;
    friend struct SetPaletteTypeCommand;
    friend struct MoveGroupCommand;

    KoColorSet::PaletteType detectFormat(const PkString &fileName, const PkByteArray &ba);
    void scribusParseColor(KoColorSet *set, PkXmlStreamReader *xml);
    bool loadScribusXmlPalette(KoColorSet *set, PkXmlStreamReader *xml);
    quint8 readByte(PkStream *io);
    quint16 readShort(PkStream *io);
    qint32 readInt(PkStream *io);
    float readFloat(PkStream *io);
    PkString readUnicodeString(PkStream *io, bool sizeIsInt = false);

    const KoColorProfile *loadColorProfile(std::unique_ptr<KoStore> &store,
                                           const PkString &path,
                                           const PkString &modelId,
                                           const PkString &colorDepthId);

    void saveKplGroup(PkXmlDocument &doc, PkXmlElement &groupEle,
                      const KisSwatchGroupSP group, PkSet<const KoColorSpace *> &colorSetSet) const;
    bool loadKplProfiles(std::unique_ptr<KoStore> &store);
    bool loadKplColorset(std::unique_ptr<KoStore> &store);
    bool loadSbzSwatchbook(std::unique_ptr<KoStore> &store);
    void loadKplGroup(const PkXmlDocument &doc, const PkXmlElement &parentElement, KisSwatchGroupSP group, PkString version);
};

#endif // KOCOLORSET_P_H
