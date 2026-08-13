#include <KisImportExportBackend.h>
#include <KisImportExportFilter.h>
#include <KisImportUserFeedbackInterface.h>
#include "KisDocument.h"
#include "KisPart.h"
#include "KisMainWindow.h"
#include "kis_async_action_feedback.h"
#include "kis_config.h"
#include "dialogs/KisColorSpaceConversionDialog.h"
#include <KoFileDialog.h>
#include <KoDialog.h>
#include <kis_config_widget.h>
#include <QApplication>
#include <QVBoxLayout>

namespace {
class Feedback final : public KisImportUserFeedbackInterface {
public:
 Feedback(QWidget *parent, bool batch) : m_parent(parent), m_batch(batch) {}
 Result askUser(AskCallback callback) override { if (m_batch) return SuppressedByBatchMode; return callback(m_parent) ? Success : UserCancelled; }
private: QWidget *m_parent; bool m_batch;
};

class Backend final : public KisImportExportBackend {
public:
 explicit Backend(KisDocument *document) : m_document(document) {}
 KisDocument *document() const override { return m_document; }
 KisImageSP image() const override { return m_document->image().toStrongRef(); }
 QByteArray nativeFormatMimeType() const override { return m_document->nativeFormatMimeType(); }
 bool batchMode() const override { return m_document->fileBatchMode(); }
 KisImportUserFeedbackInterface *createImportFeedback() const override {
   return new Feedback(KisPart::instance()->currentMainwindow(), batchMode());
 }
 KisImportExportErrorCode runAction(const QString &text, std::function<KisImportExportErrorCode()> action) override {
   KisAsyncActionFeedback feedback(text, nullptr); return feedback.runAction(std::move(action));
 }
 void saveExportConfiguration(const QByteArray &mime, KisPropertiesConfigurationSP cfg) override { KisConfig(false).setExportConfiguration(mime, cfg); }
 bool askExportConfiguration(KisImportExportFilter *filter, KisPropertiesConfigurationSP cfg,
                             const QByteArray &from, const QByteArray &to, bool batch, bool,
                             bool *alsoAsKra, bool) override {
   *alsoAsKra = false;
   if (batch) return true;
   std::unique_ptr<KisConfigWidget> widget(filter->createConfigurationWidget(nullptr, from, to));
   if (!widget) return true;
   widget->setConfiguration(cfg);
   if (KisMainWindow *window = KisPart::instance()->currentMainwindow()) widget->setView(window->viewManager());
   KoDialog dialog(qApp->activeWindow()); dialog.setMainWidget(widget.get()); widget.release();
   dialog.setButtons(KoDialog::Ok | KoDialog::Cancel);
   if (!dialog.exec()) return false;
   if (cfg) *cfg = *qobject_cast<KisConfigWidget *>(dialog.mainWidget())->configuration();
   return true;
 }
 void setErrorMessage(const QString &message) override { m_document->setErrorMessage(message); }
private: KisDocument *m_document;
};

class Services final : public KisImportExportUiServices {
public:
 QString askForAudioFileName(const QString &dir, QWidget *parent) override {
   KoFileDialog dialog(parent, KoFileDialog::ImportFiles, "ImportAudio"); if (!dir.isEmpty()) dialog.setDefaultDir(dir);
   dialog.setMimeTypeFilters({"audio/mpeg", "audio/ogg", "audio/vorbis", "audio/vnd.wave", "audio/flac"}); return dialog.filename();
 }
 QString getUriForAdditionalFile(const QString &uri, QWidget *parent) override {
   KoFileDialog dialog(parent, KoFileDialog::SaveFile, "Save Kra"); dialog.setDirectoryUrl(QUrl(uri));
   dialog.setMimeTypeFilters({"application/x-krita"}); return dialog.filename();
 }
 QString exportConfigurationXml(const QByteArray &mime) override { return KisConfig(true).exportConfigurationXML(mime); }
 bool chooseColorSpace(QWidget *parent, const KoColorSpace *fallback, const KoColorSpace **space, int *intent, int *flags) override {
   KisColorSpaceConversionDialog dialog(parent, "ColorSpaceConversion"); dialog.setInitialColorSpace(fallback, 0);
   if (dialog.exec() != QDialog::Accepted) return false;
   *space = dialog.colorSpace(); *intent = int(dialog.conversionIntent()); *flags = int(dialog.conversionFlags()); return true;
 }
};
Services s_services;
struct Registration { Registration() { setKisImportExportBackendFactory([](KisDocument *d) { return std::make_unique<Backend>(d); }); setKisImportExportUiServices(&s_services); } } s_registration;
}
