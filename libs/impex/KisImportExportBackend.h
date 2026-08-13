#ifndef KIS_IMPORT_EXPORT_BACKEND_H
#define KIS_IMPORT_EXPORT_BACKEND_H
#include <functional>
#include <memory>
#include <QByteArray>
#include <QString>
#include <kis_types.h>
#include <kis_properties_configuration.h>
#include "kritaimpex_export.h"
#include "KisImportExportErrorCode.h"
class KisDocument; class KisImportExportFilter; class KisImportUserFeedbackInterface; class QWidget; class KoColorSpace;
class KRITAIMPEX_EXPORT KisImportExportBackend {
public:
 virtual ~KisImportExportBackend() = default;
 virtual KisDocument *document() const = 0; virtual KisImageSP image() const = 0;
 virtual QByteArray nativeFormatMimeType() const = 0; virtual bool batchMode() const = 0;
 virtual KisImportUserFeedbackInterface *createImportFeedback() const = 0;
 virtual KisImportExportErrorCode runAction(const QString &, std::function<KisImportExportErrorCode()>) = 0;
 virtual void saveExportConfiguration(const QByteArray &, KisPropertiesConfigurationSP) = 0;
 virtual bool askExportConfiguration(KisImportExportFilter *, KisPropertiesConfigurationSP, const QByteArray &, const QByteArray &, bool, bool, bool *, bool) = 0;
 virtual void setErrorMessage(const QString &) = 0;
};
using KisImportExportBackendFactory = std::function<std::unique_ptr<KisImportExportBackend>(KisDocument *)>;
KRITAIMPEX_EXPORT void setKisImportExportBackendFactory(KisImportExportBackendFactory);
KRITAIMPEX_EXPORT std::unique_ptr<KisImportExportBackend> createKisImportExportBackend(KisDocument *);
class KRITAIMPEX_EXPORT KisImportExportUiServices {
public:
 virtual ~KisImportExportUiServices() = default;
 virtual QString askForAudioFileName(const QString &, QWidget *) = 0;
 virtual QString getUriForAdditionalFile(const QString &, QWidget *) = 0;
 virtual QString exportConfigurationXml(const QByteArray &) = 0;
 virtual bool chooseColorSpace(QWidget *, const KoColorSpace *, const KoColorSpace **, int *, int *) = 0;
};
KRITAIMPEX_EXPORT void setKisImportExportUiServices(KisImportExportUiServices *);
KRITAIMPEX_EXPORT KisImportExportUiServices *kisImportExportUiServices();
#endif
