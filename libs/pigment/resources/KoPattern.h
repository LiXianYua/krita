/*
    SPDX-FileCopyrightText: 2000 Matthias Elter <elter@kde.org>

    SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef KOPATTERN_H
#define KOPATTERN_H

#include <PkImage.h>
#include <PkPair.h>
#include <PkSharedPointer.h>
#include <PkString.h>

#include <KoResource.h>
#include <KisResourceTypes.h>
#include <kritapigment_export.h>

class PkStream;

class KoPattern;
typedef PkSharedPointer<KoPattern> KoPatternSP;


/// Write API docs here
class KRITAPIGMENT_EXPORT KoPattern : public KoResource
{
public:

    /**
     * Creates a new KoPattern object using @p filename.  No file is opened
     * in the constructor, you have to call load.
     *
     * @param filename the file name to save and load from.
     */
    explicit KoPattern(const PkString &filename);

    /**
     * Create a new pattern from scratch, without loading it from a file
     *
     * @param image the pattern
     * @param name the name of the pattern
     * @param filename the filename of the pattern (note that this filename does not need to exist)
     */
    KoPattern(const PkImage &image, const PkString &name, const PkString &filename);
    ~KoPattern() override;

    KoPattern(const KoPattern &rhs);
    KoPattern& operator=(const KoPattern& rhs) = delete;
    KoResourceSP clone() const override;


public:

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream* dev) const override;

    bool loadPatFromDevice(PkStream *dev);
    bool savePatToDevice(PkStream* dev) const;

    qint32 width() const;
    qint32 height() const;

    PkString defaultFileExtension() const override;

    PkPair<PkString, PkString> resourceType() const override {
        return PkPair<PkString, PkString>(ResourceType::Patterns, "");
    }

    /**
     * @brief pattern the actual pattern image
     * @return a valid PkImage. There are no guarantees to the image format.
     */
    PkImage pattern() const;

    bool hasAlpha() const;

    /**
     * Create a copy of this pattern removing all the transparency from
     * it. The fully transparent color becomes 100% black. The name and the
     * filename of the new pattern are kept the same.
     *
     * If hasAlpha() is false, the function just returns a simple clone
     * of this pattern.
     */
    KoPatternSP cloneWithoutAlpha() const;

private:

    void setPatternImage(const PkImage& image);
    void checkForAlpha(const PkImage& image);

private:
    PkImage m_pattern;
    bool m_hasAlpha = false;
};

#endif // KOPATTERN_H
