#include <KisImportExportBackend.h>
#include <KisImportExportFilter.h>
#include <KisImportUserFeedbackInterface.h>
#include "KisDocument.h"
#include "KisPart.h"
#include "KisMainWindow.h"
#include "KisReferenceImagesLayer.h"
#include "imagesize/wdg_imagesize.h"
#include "kis_async_action_feedback.h"
#include "kis_config.h"
#include "kis_grid_config.h"
#include "kis_guides_config.h"
#include "dialogs/KisColorSpaceConversionDialog.h"
#include <KisExportCheckBase.h>
#include <KisMimeDatabase.h>
#include <KisPopupButton.h>
#include <KisPreExportChecker.h>
#include <KoFileDialog.h>
#include <KoDialog.h>
#include <kis_adjustment_layer.h>
#include <kis_assert.h>
#include <kis_config_widget.h>
#include <kis_filter_mask.h>
#include <kis_icon_utils.h>
#include <kis_layer.h>
#include <kis_layer_utils.h>
#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QThread>
#include <QVBoxLayout>

namespace {
class Feedback final : public KisImportUserFeedbackInterface {
public:
 Feedback(QWidget *parent, bool batch) : m_parent(parent), m_batch(batch) {}
 Result askUser(AskCallback callback) override {
   if (m_batch) return SuppressedByBatchMode;
   KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(m_parent, SuppressedByBatchMode);
   return callback(m_parent) ? Success : UserCancelled;
 }
private: QWidget *m_parent; bool m_batch;
};

class Backend final : public KisImportExportBackend {
public:
 explicit Backend(KisDocument *document) : m_document(document) {}
 KisDocument *document() const override { return m_document; }
 KisImageSP image() const override { return m_document->image().toStrongRef(); }
 KisImageSP savingImage() const override { return m_document->savingImage(); }
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
                             const QByteArray &from, const QByteArray &to, bool batchMode,
                             bool showWarnings, bool *alsoAsKra,
                             bool isAdvancedExporting) override {
   const QString mimeUserDescription = KisMimeDatabase::descriptionForMimeType(to);

   QStringList warnings;
   QStringList errors;

   {
     KisPreExportChecker checker;
     checker.check(m_document->image(), filter->exportChecks());

     warnings = checker.warnings();
     errors = checker.errors();
   }

   KisConfigWidget *wdg = nullptr;

   if (QThread::currentThread() == qApp->thread()) {
     wdg = filter->createConfigurationWidget(nullptr, from, to);

     KisMainWindow *kisMain = KisPart::instance()->currentMainwindow();
     if (wdg && kisMain) {
       wdg->setView(kisMain->viewManager());
     }
   }

   if (!m_document->assistants().isEmpty() && to != m_document->nativeFormatMimeType()) {
     warnings.append(i18nc("image conversion warning", "The image contains <b>assistants</b>. The assistants will not be saved."));
   }
   if (m_document->referenceImagesLayer() && m_document->referenceImagesLayer()->shapeCount() > 0 && to != m_document->nativeFormatMimeType()) {
     warnings.append(i18nc("image conversion warning", "The image contains <b>reference images</b>. The reference images will not be saved."));
   }
   if (m_document->guidesConfig().hasGuides() && !filter->exportSupportsGuides()) {
     warnings.append(i18nc("image conversion warning", "The image contains <b>guides</b>. The guides will not be saved."));
   }
   if (!m_document->gridConfig().isDefault() && to != m_document->nativeFormatMimeType()) {
     warnings.append(i18nc("image conversion warning", "The image contains a <b>custom grid configuration</b>. The configuration will not be saved."));
   }

   bool shouldFlattenTheImageBeforeScaling = false;

   if (isAdvancedExporting) {
     QMap<QString, KisExportCheckBase *> exportChecks = filter->exportChecks();

     const bool filterSupportsMultilayerExport =
         exportChecks.contains("MultiLayerCheck") &&
         exportChecks["MultiLayerCheck"]->checkNeeded(m_document->image()) &&
         exportChecks["MultiLayerCheck"]->check(m_document->image()) == KisExportCheckBase::SUPPORTED;

     if (!filterSupportsMultilayerExport) {
       shouldFlattenTheImageBeforeScaling = true;
     } else {
       if (KisLayerUtils::findNodeByType<KisAdjustmentLayer>(m_document->image()->root())) {
         shouldFlattenTheImageBeforeScaling = true;
         warnings.append(i18nc("image conversion warning", "Trying to perform an Advanced Export with the image containing a <b>filter layer</b>. The image will be <b>flattened</b> before resizing."));
       }
       if (KisLayerUtils::findNodeByType<KisFilterMask>(m_document->image()->root())) {
         shouldFlattenTheImageBeforeScaling = true;
         warnings.append(i18nc("image conversion warning", "Trying to perform an Advanced Export with the image containing a <b>filter mask</b>. The image will be <b>flattened</b> before resizing."));
       }
       const bool hasLayerStyles =
           KisLayerUtils::recursiveFindNode(m_document->image()->root(), [] (KisNodeSP node) {
             KisLayer *layer = dynamic_cast<KisLayer *>(node.data());
             return layer && layer->layerStyle();
           });

       if (hasLayerStyles) {
         shouldFlattenTheImageBeforeScaling = true;
         warnings.append(i18nc("image conversion warning", "Trying to perform an Advanced Export with the image containing a <b>layer style</b>. The image will be <b>flattened</b> before resizing."));
       }
     }
   }

   if (!batchMode && !errors.isEmpty()) {
     QString error = "<html><body><p><b>"
         + i18n("Error: cannot save this image as a %1.", mimeUserDescription)
         + "</b> " + i18n("Reasons:") + "</p>"
         + "<p/><ul>";
     for (const QString &item : errors) {
       error += "\n<li>" + item + "</li>";
     }
     error += "</ul>";

     QMessageBox::critical(KisPart::instance()->currentMainwindow(), i18nc("@title:window", "Krita: Export Error"), error);
     return false;
   }

   if (!batchMode && (wdg || !warnings.isEmpty() || isAdvancedExporting)) {
     QWidget *page = new QWidget();
     QVBoxLayout *layout = new QVBoxLayout(page);

     if (showWarnings && !warnings.isEmpty()) {
       QHBoxLayout *hLayout = new QHBoxLayout();

       QLabel *labelWarning = new QLabel();
       labelWarning->setPixmap(KisIconUtils::loadIcon("dialog-warning").pixmap(48, 48));
       hLayout->addWidget(labelWarning);

       KisPopupButton *button = new KisPopupButton(nullptr);
       button->setText(i18nc("Keep the extra space at the end of the sentence, please", "Warning: saving as a %1 will lose information from your image.    ", mimeUserDescription));
       button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
       hLayout->addWidget(button);
       layout->addLayout(hLayout);

       QTextBrowser *browser = new QTextBrowser();
       browser->setMinimumWidth(button->width());
       button->setPopupWidget(browser);

       QString warning = "<html><body><p><b>"
           + i18n("You will lose information when saving this image as a %1.", mimeUserDescription);
       warning += warnings.size() == 1
           ? "</b> " + i18n("Reason:") + "</p>"
           : "</b> " + i18n("Reasons:") + "</p>";
       warning += "<p/><ul>";
       for (const QString &item : warnings) {
         warning += "\n<li>" + item + "</li>";
       }
       warning += "</ul>";
       browser->setHtml(warning);
     }

     QTabWidget *box = new QTabWidget();
     if (wdg) {
       wdg->setConfiguration(cfg);
       box->addTab(wdg, i18n("Options"));
     }

     WdgImageSize *wdgImageSize = nullptr;
     if (isAdvancedExporting) {
       wdgImageSize = new WdgImageSize(box, m_document->image()->width(), m_document->image()->height(), m_document->image()->yRes());
       box->addTab(wdgImageSize, i18n("Resize"));
     }
     layout->addWidget(box);

     QCheckBox *chkAlsoAsKra = nullptr;
     if (showWarnings && !warnings.isEmpty()) {
       chkAlsoAsKra = new QCheckBox(i18n("Also save your image as a Krita file."));
       chkAlsoAsKra->setChecked(KisConfig(true).readEntry<bool>("AlsoSaveAsKra", false));
       layout->addWidget(chkAlsoAsKra);
     }

     KoDialog dialog(qApp->activeWindow());
     dialog.setMainWidget(page);
     page->setParent(&dialog);
     dialog.setButtons(KoDialog::Ok | KoDialog::Cancel);
     dialog.setWindowTitle(mimeUserDescription);

     if (showWarnings || wdg || isAdvancedExporting) {
       if (!dialog.exec()) {
         return false;
       }
     }

     *alsoAsKra = false;
     if (chkAlsoAsKra) {
       KisConfig(false).writeEntry<bool>("AlsoSaveAsKra", chkAlsoAsKra->isChecked());
       *alsoAsKra = chkAlsoAsKra->isChecked();
     }

     KIS_SAFE_ASSERT_RECOVER_NOOP(bool(isAdvancedExporting) == bool(wdgImageSize));

     if (isAdvancedExporting && wdgImageSize) {
       if (shouldFlattenTheImageBeforeScaling) {
         m_document->savingImage()->flatten(KisNodeSP());
         m_document->savingImage()->waitForDone();
       }

       const QSize desiredSize(wdgImageSize->desiredWidth(), wdgImageSize->desiredHeight());
       const double resolution = wdgImageSize->desiredResolution();
       m_document->savingImage()->scaleImage(desiredSize, resolution, resolution, wdgImageSize->filterType());
       m_document->savingImage()->waitForDone();
       KisLayerUtils::forceAllDelayedNodesUpdate(m_document->savingImage()->root());
       m_document->savingImage()->waitForDone();
     }

     if (wdg) {
       *cfg = *wdg->configuration();
     }
   }

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
