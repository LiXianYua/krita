#ifndef KIS_IMPORT_EXPORT_BACKEND_H
#define KIS_IMPORT_EXPORT_BACKEND_H
#include <functional>
#include <memory>
#include <PkAuxTypes.h>
#include <PkString.h>
#include <kis_types.h>
#include <kis_properties_configuration.h>
#include "kritaimpex_export.h"
#include "KisImportExportErrorCode.h"
class KisDocument; class KisImportExportFilter; class KisImportUserFeedbackInterface; class PkWidget; class KoColorSpace;
class KRITAIMPEX_EXPORT KisImportExportBackend {
public:
 virtual ~KisImportExportBackend() = default;
 virtual KisDocument *document() const = 0; virtual KisImageSP image() const = 0;
 virtual KisImageSP savingImage() const = 0;
 virtual PkByteArray nativeFormatMimeType() const = 0; virtual bool batchMode() const = 0;
 virtual KisImportUserFeedbackInterface *createImportFeedback() const = 0;
 virtual KisImportExportErrorCode runAction(const PkString &, std::function<KisImportExportErrorCode()>) = 0;
 virtual void saveExportConfiguration(const PkByteArray &, KisPropertiesConfigurationSP) = 0;
 virtual bool askExportConfiguration(KisImportExportFilter *, KisPropertiesConfigurationSP, const PkByteArray &, const PkByteArray &, bool, bool, bool *, bool) = 0;
 virtual void setErrorMessage(const PkString &) = 0;
};
using KisImportExportBackendFactory = std::function<std::unique_ptr<KisImportExportBackend>(KisDocument *)>;
KRITAIMPEX_EXPORT void setKisImportExportBackendFactory(KisImportExportBackendFactory);
KRITAIMPEX_EXPORT std::unique_ptr<KisImportExportBackend> createKisImportExportBackend(KisDocument *);
KRITAIMPEX_EXPORT KisImageSP kisImportExportSavingImage(KisDocument *);
class KRITAIMPEX_EXPORT KisImportExportUiServices {
public:
 virtual ~KisImportExportUiServices() = default;
 virtual PkString askForAudioFileName(const PkString &, PkWidget *) = 0;
 virtual PkString getUriForAdditionalFile(const PkString &, PkWidget *) = 0;
 virtual PkString exportConfigurationXml(const PkByteArray &) = 0;
 virtual bool chooseColorSpace(PkWidget *, const KoColorSpace *, const KoColorSpace **, int *, int *) = 0;
};
KRITAIMPEX_EXPORT void setKisImportExportUiServices(KisImportExportUiServices *);
KRITAIMPEX_EXPORT KisImportExportUiServices *kisImportExportUiServices();
#endif
