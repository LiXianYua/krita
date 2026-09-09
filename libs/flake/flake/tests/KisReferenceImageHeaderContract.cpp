/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisReferenceImage.h>
#include <KoShapeUserData.h>
#include <KoSelection.h>
#include <KoCanvasBase.h>
#include <KoShapeManager.h>
#include <KisCanvasFeedback.h>
#include <kis_dummies_facade_base.h>
#include <kis_node_dummies_graph.h>
#include <kis_node_shape.h>
#include <kis_shape_layer.h>
#include <kis_shape_selection.h>
#include <kis_shape_selection_model.h>
#include <kis_qimage_pyramid.h>

#include <QObject>
#include <type_traits>

namespace {

using HeadlessPaintDeviceFactory = KisReferenceImage *(*)(
    KisPaintDeviceSP, const KisCoordinatesConverter &);
using PkCanvasObserverDisconnect = void (KoCanvasBase::*)(PkObject *);
using PkFloatingMessage = void (KisCanvasFeedback::*)(
    const PkString &, int, KisCanvasFeedback::Priority, int);

template <typename T, typename = void>
struct HasLegacyIconMethod : std::false_type {};

template <typename T>
struct HasLegacyIconMethod<T, std::void_t<decltype(&T::icon)>> : std::true_type {};

static_assert(std::is_same_v<decltype(&KisReferenceImage::fromPaintDevice),
                             HeadlessPaintDeviceFactory>);
static_assert(std::is_same_v<decltype(&KoCanvasBase::disconnectCanvasObserver),
                             PkCanvasObserverDisconnect>);
static_assert(std::is_same_v<decltype(&KisCanvasFeedback::showFloatingMessage),
                             PkFloatingMessage>);
static_assert(!HasLegacyIconMethod<KisShapeLayer>::value);
static_assert(std::is_constructible_v<KisImagePyramid, const PkImage &, bool>);

static_assert(!std::is_base_of_v<QObject, KoShapeUserData>);

static_assert(!std::is_base_of_v<QObject, KisNodeDummy>);
static_assert(std::is_base_of_v<PkObject, KisNodeDummy>);
static_assert(!std::is_base_of_v<QObject, KisDummiesFacadeBase>);
static_assert(std::is_base_of_v<PkObject, KisDummiesFacadeBase>);
static_assert(!std::is_base_of_v<QObject, KisNodeShape>);
static_assert(std::is_base_of_v<PkObject, KisNodeShape>);
static_assert(!std::is_base_of_v<QObject, KisShapeSelection>);
static_assert(std::is_base_of_v<PkObject, KisShapeSelection>);
static_assert(!std::is_base_of_v<QObject, KisShapeSelectionModel>);
static_assert(std::is_base_of_v<PkObject, KisShapeSelectionModel>);
static_assert(!std::is_base_of_v<QObject, KoSelection>);
static_assert(std::is_base_of_v<PkObject, KoSelection>);
static_assert(!std::is_base_of_v<QObject, KoShapeManager>);
static_assert(std::is_base_of_v<PkObject, KoShapeManager>);
static_assert(!std::is_base_of_v<QObject, KoCanvasBase>);
static_assert(std::is_base_of_v<PkObject, KoCanvasBase>);

}
