/*
 *  SPDX-FileCopyrightText: 2013 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_FILE_LAYER_H
#define KIS_FILE_LAYER_H

#include <functional>

#include "kritaimage_export.h"

#include "kis_safe_document_loader.h"
#include "kis_external_layer_iface.h"
#include <PkConnection.h>
#include <PkSize.h>
#include <PkString.h>
#include <PkTransform.h>

/**
 * @brief The KisFileLayer class loads a particular file as a layer into the layer stack.
 */
class KRITAIMAGE_EXPORT KisFileLayer : public KisExternalLayer
{
public:
    using FileOpener = std::function<void(const PkString &path)>;

    enum ScalingMethod {
        None,
        ToImageSize,
        ToImagePPI
    };

    KisFileLayer(KisImageWSP image, const PkString &name, quint8 opacity);
    /**
     * @brief KisFileLayer create a new file layer with the given file
     * @param image the image the file layer will belong to
     * @param basePath the path to the image, if it has been saved before.
     * @param filename the path to the file, relative to the basePath
     * @param scalingMethod @see ScalingMethod
     * @param scalingFilter the ID of the KisFilterStrategy to be used if scaling
     * @param name the name of the layer
     * @param opacity the opacity of the layer
     */
    KisFileLayer(KisImageWSP image, const PkString& basePath, const PkString &filename, ScalingMethod scalingMethod, PkString scalingFilter, const PkString &name, quint8 opacity, const KoColorSpace *fallbackColorSpace = 0);
    ~KisFileLayer() override;
    KisFileLayer(const KisFileLayer& rhs);

    static void setDefaultFileOpener(FileOpener fileOpener);

    void resetCache(const KoColorSpace *colorSpace = 0) override;

    KisPaintDeviceSP original() const override;
    KisPaintDeviceSP paintDevice() const override;
    void setSectionModelProperties(const KisBaseNode::PropertyList &properties) override;
    KisBaseNode::PropertyList sectionModelProperties() const override;

    /**
     * @brief setFileName replace the existing file with a new one
     * @param basePath the path to the image, if it has been saved before.
     * @param filename the path to the file, relative to the basePath
     */
    void setFileName(const PkString &basePath, const PkString &filename);
    PkString fileName() const;
    PkString path() const;


    ScalingMethod scalingMethod() const;
    void setScalingMethod(ScalingMethod method);

    PkString scalingFilter() const;
    void setScalingFilter(PkString method);

    KisNodeSP clone() const override;
    bool allowAsChild(KisNodeSP) const override;

    bool accept(KisNodeVisitor&) override;
    void accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter) override;

    KUndo2Command* crop(const PkRect & rect) override;
    KUndo2Command* transform(const PkTransform &transform) override;

    void setImage(KisImageWSP image) override;

private:
    void slotLoadingFinished(KisPaintDeviceSP projection, qreal xRes, qreal yRes, PkSize size);
    void slotLoadingFailed();
    void slotFileExistsStateChanged(bool exists);
    void openFile() const;
    void slotImageSizeChanged();
    void slotImageResolutionChanged();

    enum State {
        FileLoaded,
        FileNotFound,
        FileLoadingFailed
    };

    void changeState(State newState);

private:
    PkString m_basePath;
    PkString m_filename;
    ScalingMethod m_scalingMethod {None};
    PkString m_scalingFilter;

    KisPaintDeviceSP m_paintDevice;
    KisSafeDocumentLoader m_loader;
    PkSize m_generatedForImageSize;
    qreal m_generatedForXRes = 0.0;
    qreal m_generatedForYRes = 0.0;

    State m_state = FileNotFound;

    PkConnection m_imageSizeConnection;
    PkConnection m_imageResolutionConnection;
};

#endif // KIS_FILE_LAYER_H
