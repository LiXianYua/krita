/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisRequiredResourcesOperators.h"

#include <KisLocalStrokeResources.h>
#include <KisResourceLoaderRegistry.h>
#include <KisMimeDatabase.h>
#include <PkMemoryStream.h>
#include <PkThread.h>

#include "ResourceDebug.h"

bool KisRequiredResourcesOperators::detail::isLocalResourcesStorage(KisResourcesInterfaceSP resourcesInterface)
{
    return !resourcesInterface.dynamicCast<KisLocalStrokeResources>().isNull();
}

bool KisRequiredResourcesOperators::detail::assertInGuiThread()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(PkThread::currentThreadId() == PkThread::mainThreadId(), false);
    return true;
}

KisResourcesInterfaceSP KisRequiredResourcesOperators::detail::createLocalResourcesStorage(const PkList<KoResourceSP> &resources)
{
    return PkSharedPointer<KisLocalStrokeResources>::create(resources);
}

void KisRequiredResourcesOperators::detail::addResourceOrWarnIfNotLoaded(KoResourceLoadResult loadedResource, PkList<KoResourceSP> *resources, KisResourcesInterfaceSP resourcesInterface)
{
    switch (loadedResource.type()) {
    case KoResourceLoadResult::ExistingResource:
        KIS_SAFE_ASSERT_RECOVER(loadedResource.resource())
        {
            // XXX: remove once KoResourceLoadResult takes std::monostate
            qWarning() << "Attempt to retrieve a resource that is null";
            return;
        }
        resources->append(loadedResource.resource());
        break;
    case KoResourceLoadResult::EmbeddedResource: {
        /**
         * Some resources, like filter configurations may be assigned to the
         * layers without being loaded to the resource system. In such a case,
         * the embedded resources will be loaded here, when we make a snapshot.
         */

        const KoEmbeddedResource embeddedResource = loadedResource.embeddedResource();
        const KoResourceSignature sig = embeddedResource.signature();

        KisResourceLoaderBase *loader = KisResourceLoaderRegistry::instance()->loader(sig.type, KisMimeDatabase::mimeTypeForFile(sig.filename));

        if (!loader) {
            qWarning() << "createLocalResourcesSnapshot: Could not create a loader for resource" << sig;
            return;
        }

        const PkByteArray ba = embeddedResource.data();
        PkMemoryStream buf;
        buf.open(static_cast<PkStream::OpenMode>(PkStream::ReadWrite | PkStream::Truncate));
        if (buf.write(ba.constData(), ba.size()) != ba.size()) {
            qWarning() << "createLocalResourcesSnapshot: Could not buffer embedded resource" << sig;
            return;
        }
        buf.close();
        if (!buf.open(PkStream::ReadOnly)) {
            qWarning() << "createLocalResourcesSnapshot: Could not reopen embedded resource" << sig;
            return;
        }

        KoResourceSP resource = loader->load(sig.filename, buf, resourcesInterface);

        if (resource) {
            resource->setMD5Sum(sig.md5sum);
            resource->setVersion(0);
            resource->setDirty(false);

            resources->append(resource);
        } else {
            qWarning() << "createLocalResourcesSnapshot: Could not import embedded resource" << sig;
        }
        break;
    }
    case KoResourceLoadResult::FailedLink:
        qWarning() << "createLocalResourcesSnapshot: failed to load a linked resource:" << loadedResource.signature();
        break;
    }
}
