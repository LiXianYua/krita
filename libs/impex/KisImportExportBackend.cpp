#include "KisImportExportBackend.h"
#include "KisDocument.h"
#include "KisImportUserFeedbackInterface.h"

#include <kis_image.h>

namespace
{
class HeadlessFeedback final : public KisImportUserFeedbackInterface
{
public:
    Result askUser(AskCallback) override
    {
        return SuppressedByBatchMode;
    }
};

HeadlessFeedback s_headlessFeedback;

class HeadlessBackend final : public KisImportExportBackend
{
public:
    explicit HeadlessBackend(KisDocument *document)
        : m_document(document)
    {
    }

    KisDocument *document() const override { return m_document; }
    KisImageSP image() const override { return m_document->image().toStrongRef(); }
    KisImageSP savingImage() const override { return m_document->savingImage(); }
    PkByteArray nativeFormatMimeType() const override { return m_document->nativeFormatMimeType(); }
    bool batchMode() const override { return m_document->fileBatchMode(); }
    KisImportUserFeedbackInterface *createImportFeedback() const override
    {
        return &s_headlessFeedback;
    }
    KisImportExportErrorCode runAction(
        const PkString &,
        std::function<KisImportExportErrorCode()> action) override
    {
        return action();
    }
    void saveExportConfiguration(const PkByteArray &, KisPropertiesConfigurationSP) override {}
    bool askExportConfiguration(KisImportExportFilter *,
                                KisPropertiesConfigurationSP,
                                const PkByteArray &,
                                const PkByteArray &,
                                bool,
                                bool,
                                bool *alsoAsKra,
                                bool) override
    {
        if (alsoAsKra) {
            *alsoAsKra = false;
        }
        return true;
    }
    void setErrorMessage(const PkString &message) override
    {
        m_document->setErrorMessage(message);
    }

private:
    KisDocument *m_document;
};

class HeadlessUiServices final : public KisImportExportUiServices
{
public:
    PkString askForAudioFileName(const PkString &, PkWidget *) override { return {}; }
    PkString getUriForAdditionalFile(const PkString &, PkWidget *) override { return {}; }
    PkString exportConfigurationXml(const PkByteArray &) override { return {}; }
    bool chooseColorSpace(PkWidget *, const KoColorSpace *, const KoColorSpace **,
                          int *, int *) override
    {
        return false;
    }
};

KisImportExportBackendFactory s_factory;
KisImportExportUiServices *s_services = nullptr;
HeadlessUiServices s_headlessUiServices;
}

void setKisImportExportBackendFactory(KisImportExportBackendFactory factory) { s_factory = std::move(factory); }
std::unique_ptr<KisImportExportBackend> createKisImportExportBackend(KisDocument *document)
{
    return s_factory ? s_factory(document) : std::make_unique<HeadlessBackend>(document);
}
KisImageSP kisImportExportSavingImage(KisDocument *document) {
    std::unique_ptr<KisImportExportBackend> backend = createKisImportExportBackend(document);
    return backend ? backend->savingImage() : KisImageSP();
}
void setKisImportExportUiServices(KisImportExportUiServices *services) { s_services = services; }
KisImportExportUiServices *kisImportExportUiServices()
{
    return s_services ? s_services : &s_headlessUiServices;
}
