/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_REFERENCE_IMAGE_TOOL_SERVICES_H
#define KIS_REFERENCE_IMAGE_TOOL_SERVICES_H

#include <kritacanvas_export.h>

class KisDocument;
class KisReferenceImage;
class QString;
class QWidget;

/**
 * Narrow host services used by the reference-images tool.
 *
 * Shape manipulation remains in the tool and document domains. These methods
 * cover only the active-view services which KoCanvasBase cannot represent.
 */
class KRITACANVAS_EXPORT KisReferenceImageToolServices
{
public:
    virtual ~KisReferenceImageToolServices();

    virtual KisDocument *referenceImageDocument() const = 0;
    virtual QWidget *referenceImageDialogParent() const = 0;
    virtual KisReferenceImage *referenceImageFromFile(const QString &filename) = 0;
    virtual KisReferenceImage *referenceImageFromClipboard() = 0;
    virtual void createReferenceImageFromLayer() = 0;
    virtual void createReferenceImageFromVisible() = 0;
};

#endif // KIS_REFERENCE_IMAGE_TOOL_SERVICES_H
