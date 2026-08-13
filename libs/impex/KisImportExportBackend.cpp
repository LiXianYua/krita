#include "KisImportExportBackend.h"
namespace { KisImportExportBackendFactory s_factory; KisImportExportUiServices *s_services = nullptr; }
void setKisImportExportBackendFactory(KisImportExportBackendFactory factory) { s_factory = std::move(factory); }
std::unique_ptr<KisImportExportBackend> createKisImportExportBackend(KisDocument *document) { return s_factory ? s_factory(document) : nullptr; }
void setKisImportExportUiServices(KisImportExportUiServices *services) { s_services = services; }
KisImportExportUiServices *kisImportExportUiServices() { return s_services; }
