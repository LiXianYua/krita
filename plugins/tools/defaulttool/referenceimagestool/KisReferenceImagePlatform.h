/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Public host-neutral platform and serialization contract for reference images.
 */
#pragma once

#include <PkString.h>
#include <PkStringList.h>

class KisDocument;
class KisReferenceImage;
class KoStore;

struct KisReferenceImageWireRecord {
    bool embedded {true};
    PkString source;
    PkString width {"100"};
    PkString height {"100"};
    PkString keepAspectRatio {"true"};
    PkString transform;
    PkString opacity {"1"};
    PkString saturation {"1"};
};

class KisReferenceImageCodec
{
public:
    virtual ~KisReferenceImageCodec() = default;

    virtual bool describeReferenceImage(KisReferenceImage *image,
                                        KisReferenceImageWireRecord *wire) const = 0;
    virtual bool saveReferenceImagePayload(KisReferenceImage *image,
                                           const PkString &internalFile,
                                           KoStore *store) const = 0;
    virtual KisReferenceImage *loadReferenceImage(const KisReferenceImageWireRecord &wire,
                                                  KoStore *store) const = 0;
};

enum class KisReferenceImageFileRequest {
    OpenImage,
    OpenCollection,
    SaveCollection
};

enum class KisReferenceImageError {
    ClipboardLoadFailed,
    CollectionOpenFailed,
    CollectionPartialLoad,
    CollectionLoadFailed,
    CollectionSaveOpenFailed,
    CollectionSaveFailed
};

class KisReferenceImagePlatformServices
{
public:
    virtual ~KisReferenceImagePlatformServices() = default;

    virtual KisDocument *referenceImageDocument() const = 0;
    virtual PkString chooseReferenceImageFile(KisReferenceImageFileRequest request) = 0;
    virtual KisReferenceImage *referenceImageFromFile(const PkString &filename) = 0;
    virtual KisReferenceImage *referenceImageFromClipboard() = 0;
    virtual void setReferenceImageClipboard(KisReferenceImage *reference) = 0;
    virtual void createReferenceImageFromLayer() = 0;
    virtual void createReferenceImageFromVisible() = 0;
    virtual void showReferenceImageError(KisReferenceImageError error,
                                         const PkString &detail = {},
                                         const PkStringList &failures = {}) = 0;
    virtual KisReferenceImageCodec &referenceImageCodec() = 0;
};
