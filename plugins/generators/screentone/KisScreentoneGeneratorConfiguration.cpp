/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkStringList.h>
#include <PkMutex.h>


#include "KisScreentoneGeneratorTemplate.h"
#include "KisScreentoneGeneratorConfiguration.h"

PkStringList screentonePatternNames()
{
    return PkStringList()
        << "Dots"
        << "Lines";
}

PkStringList screentoneShapeNames(int pattern)
{
    if (pattern == KisScreentonePatternType_Dots) {
        return PkStringList()
            << "Round"
            << "Ellipse (Legacy)"
            << "Ellipse"
            << "Diamond"
            << "Square";
    } else if (pattern == KisScreentonePatternType_Lines) {
        return PkStringList()
            << "Straight"
            << "Sine Wave"
            << "Triangular Wave"
            << "Sawtooth Wave"
            << "Curtains";
    }
    
    return PkStringList();
}

PkStringList screentoneInterpolationNames(int pattern, int shape)
{
    if (pattern == KisScreentonePatternType_Dots) {
        if (shape == KisScreentoneShapeType_RoundDots ||
            shape == KisScreentoneShapeType_EllipseDots ||
            shape == KisScreentoneShapeType_EllipseDotsLegacy) {
            return PkStringList()
                << "Linear"
                << "Sinusoidal";
        }
    } else if (pattern == KisScreentonePatternType_Lines) {
        return PkStringList()
            << "Linear"
            << "Sinusoidal";
    }

    return PkStringList();
}

class KisScreentoneGeneratorConfiguration::Private
{
public:
    Private(KisScreentoneGeneratorConfiguration *q);
    ~Private();

    const KisScreentoneGeneratorTemplate& getTemplate() const;
    void invalidateTemplate();

public:
    KisScreentoneGeneratorConfiguration *m_q{nullptr};
    mutable PkSharedPointer<KisScreentoneGeneratorTemplate> m_cachedTemplate{nullptr};
    mutable PkMutex m_templateMutex;
};

KisScreentoneGeneratorConfiguration::Private::Private(KisScreentoneGeneratorConfiguration *q)
    : m_q(q)
{}

KisScreentoneGeneratorConfiguration::Private::~Private()
{}

const KisScreentoneGeneratorTemplate& KisScreentoneGeneratorConfiguration::Private::getTemplate() const
{
    PkMutexLocker ml(&m_templateMutex);
    if (!m_cachedTemplate) {
        m_cachedTemplate.reset(new KisScreentoneGeneratorTemplate(m_q));
    }
    return *m_cachedTemplate;
}

void KisScreentoneGeneratorConfiguration::Private::invalidateTemplate()
{
    PkMutexLocker ml(&m_templateMutex);
    m_cachedTemplate.reset();
}

KisScreentoneGeneratorConfiguration::KisScreentoneGeneratorConfiguration(qint32 version, KisResourcesInterfaceSP resourcesInterface)
    : KisFilterConfiguration(defaultName(), version, resourcesInterface)
    , m_d(new Private(this))
{}

KisScreentoneGeneratorConfiguration::KisScreentoneGeneratorConfiguration(KisResourcesInterfaceSP resourcesInterface)
    : KisFilterConfiguration(defaultName(), defaultVersion(), resourcesInterface)
    , m_d(new Private(this))
{}

KisScreentoneGeneratorConfiguration::KisScreentoneGeneratorConfiguration(const KisScreentoneGeneratorConfiguration &rhs)
    : KisFilterConfiguration(rhs)
    , m_d(new Private(this))
{
    m_d->m_cachedTemplate = rhs.m_d->m_cachedTemplate;
}

KisScreentoneGeneratorConfiguration::~KisScreentoneGeneratorConfiguration()
{}

KisFilterConfigurationSP KisScreentoneGeneratorConfiguration::clone() const
{
    return new KisScreentoneGeneratorConfiguration(*this);
}

int KisScreentoneGeneratorConfiguration::pattern() const
{
    return getInt("pattern", defaultPattern());
}

int KisScreentoneGeneratorConfiguration::shape() const
{
    return getInt("shape", defaultShape());
}

int KisScreentoneGeneratorConfiguration::interpolation() const
{
    return getInt("interpolation", defaultInterpolation());
}

int KisScreentoneGeneratorConfiguration::equalizationMode() const
{
    return getInt("equalization_mode", version() == 1 ? KisScreentoneEqualizationMode_None : defaultEqualizationMode());
}

KoColor KisScreentoneGeneratorConfiguration::foregroundColor() const
{
    return getColor("foreground_color", defaultForegroundColor());
}

KoColor KisScreentoneGeneratorConfiguration::backgroundColor() const
{
    return getColor("background_color", defaultBackgroundColor());
}

int KisScreentoneGeneratorConfiguration::foregroundOpacity() const
{
    return getInt("foreground_opacity", defaultForegroundOpacity());
}

int KisScreentoneGeneratorConfiguration::backgroundOpacity() const
{
    return getInt("background_opacity", defaultBackgroundOpacity());
}

bool KisScreentoneGeneratorConfiguration::invert() const
{
    return getBool("invert", defaultInvert());
}

qreal KisScreentoneGeneratorConfiguration::brightness() const
{
    return getDouble("brightness", defaultBrightness());
}

qreal KisScreentoneGeneratorConfiguration::contrast() const
{
    return getDouble("contrast", defaultContrast());
}

int KisScreentoneGeneratorConfiguration::sizeMode() const
{
    return getInt("size_mode", version() == 1 ? KisScreentoneSizeMode_PixelBased : defaultSizeMode());
}

int KisScreentoneGeneratorConfiguration::units() const
{
    return getInt("units", defaultUnits());
}

qreal KisScreentoneGeneratorConfiguration::resolution() const
{
    return getDouble("resolution", defaultResolution());
}

qreal KisScreentoneGeneratorConfiguration::frequencyX() const
{
    return getDouble("frequency_x", defaultFrequencyX());
}

qreal KisScreentoneGeneratorConfiguration::frequencyY() const
{
    return getDouble("frequency_y", defaultFrequencyY());
}

bool KisScreentoneGeneratorConfiguration::constrainFrequency() const
{
    return getBool("constrain_frequency", defaultConstrainFrequency());
}

qreal KisScreentoneGeneratorConfiguration::positionX() const
{
    return getDouble("position_x", defaultPositionX());
}

qreal KisScreentoneGeneratorConfiguration::positionY() const
{
    return getDouble("position_y", defaultPositionY());
}

qreal KisScreentoneGeneratorConfiguration::sizeX() const
{
    return getDouble("size_x", defaultSizeX());
}

qreal KisScreentoneGeneratorConfiguration::sizeY() const
{
    return getDouble("size_y", defaultSizeY());
}

bool KisScreentoneGeneratorConfiguration::constrainSize() const
{
    return getBool("keep_size_square", defaultConstrainSize());
}

qreal KisScreentoneGeneratorConfiguration::shearX() const
{
    return getDouble("shear_x", defaultShearX());
}

qreal KisScreentoneGeneratorConfiguration::shearY() const
{
    return getDouble("shear_y", defaultShearY());
}

qreal KisScreentoneGeneratorConfiguration::rotation() const
{
    return getDouble("rotation", defaultRotation());
}

bool KisScreentoneGeneratorConfiguration::alignToPixelGrid() const
{
    return getBool("align_to_pixel_grid", version() == 1 ? false : defaultAlignToPixelGrid());
}

int KisScreentoneGeneratorConfiguration::alignToPixelGridX() const
{
    return getInt("align_to_pixel_grid_x", defaultAlignToPixelGridX());
}

int KisScreentoneGeneratorConfiguration::alignToPixelGridY() const
{
    return getInt("align_to_pixel_grid_y", defaultAlignToPixelGridY());
}

const KisScreentoneGeneratorTemplate& KisScreentoneGeneratorConfiguration::getTemplate() const
{
    return m_d->getTemplate();
}

void KisScreentoneGeneratorConfiguration::setPattern(int newPattern)
{
    setProperty("pattern", newPattern);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setShape(int newShape)
{
    setProperty("shape", newShape);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setInterpolation(int newInterpolation)
{
    setProperty("interpolation", newInterpolation);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setEqualizationMode(int newEqualizationMode)
{
    setProperty("equalization_mode", newEqualizationMode);
}

void KisScreentoneGeneratorConfiguration::setForegroundColor(const KoColor &newForegroundColor)
{
    PkVariant v;
    v.setValue(newForegroundColor);
    setProperty("foreground_color", v);
}

void KisScreentoneGeneratorConfiguration::setBackgroundColor(const KoColor &newBackgroundColor)
{
    PkVariant v;
    v.setValue(newBackgroundColor);
    setProperty("background_color", v);
}

void KisScreentoneGeneratorConfiguration::setForegroundOpacity(int newForegroundOpacity)
{
    setProperty("foreground_opacity", newForegroundOpacity);
}

void KisScreentoneGeneratorConfiguration::setBackgroundOpacity(int newBackgroundOpacity)
{
    setProperty("background_opacity", newBackgroundOpacity);
}

void KisScreentoneGeneratorConfiguration::setInvert(bool newInvert)
{
    setProperty("invert", newInvert);
}

void KisScreentoneGeneratorConfiguration::setBrightness(qreal newBrightness)
{
    setProperty("brightness", newBrightness);
}

void KisScreentoneGeneratorConfiguration::setContrast(qreal newContrast)
{
    setProperty("contrast", newContrast);
}

void KisScreentoneGeneratorConfiguration::setSizeMode(int newSizeMode)
{
    setProperty("size_mode", newSizeMode);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setUnits(int newUnits)
{
    setProperty("units", newUnits);
}

void KisScreentoneGeneratorConfiguration::setResolution(qreal newResolution)
{
    setProperty("resolution", newResolution);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setFrequencyX(qreal newFrequencyX)
{
    setProperty("frequency_x", newFrequencyX);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setFrequencyY(qreal newFrequencyY)
{
    setProperty("frequency_y", newFrequencyY);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setConstrainFrequency(bool newConstrainFrequency)
{
    setProperty("constrain_frequency", newConstrainFrequency);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setPositionX(qreal newPositionX)
{
    setProperty("position_x", newPositionX);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setPositionY(qreal newPositionY)
{
    setProperty("position_y", newPositionY);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setSizeX(qreal newSizeX)
{
    setProperty("size_x", newSizeX);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setSizeY(qreal newSizeY)
{
    setProperty("size_y", newSizeY);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setConstrainSize(bool newConstrainSize)
{
    setProperty("keep_size_square", newConstrainSize);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setShearX(qreal newShearX)
{
    setProperty("shear_x", newShearX);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setShearY(qreal newShearY)
{
    setProperty("shear_y", newShearY);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setRotation(qreal newRotation)
{
    setProperty("rotation", newRotation);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setAlignToPixelGrid(bool newAlignToPixelGrid)
{
    setProperty("align_to_pixel_grid", newAlignToPixelGrid);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setAlignToPixelGridX(int newAlignToPixelGridX)
{
    setProperty("align_to_pixel_grid_x", newAlignToPixelGridX);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setAlignToPixelGridY(int newAlignToPixelGridY)
{
    setProperty("align_to_pixel_grid_y", newAlignToPixelGridY);
    m_d->invalidateTemplate();
}

void KisScreentoneGeneratorConfiguration::setDefaults()
{
    setPattern(defaultPattern());
    setShape(defaultShape());
    setInterpolation(defaultInterpolation());
    setEqualizationMode(defaultEqualizationMode());
    setForegroundColor(defaultForegroundColor());
    setBackgroundColor(defaultBackgroundColor());
    setForegroundOpacity(defaultForegroundOpacity());
    setBackgroundOpacity(defaultBackgroundOpacity());
    setInvert(defaultInvert());
    setBrightness(defaultBrightness());
    setContrast(defaultContrast());
    setSizeMode(defaultSizeMode());
    setUnits(defaultUnits());
    setResolution(defaultResolution());
    setFrequencyX(defaultFrequencyX());
    setFrequencyY(defaultFrequencyY());
    setConstrainFrequency(defaultConstrainFrequency());
    setPositionX(defaultPositionX());
    setPositionY(defaultPositionY());
    setSizeX(defaultSizeX());
    setSizeY(defaultSizeY());
    setConstrainSize(defaultConstrainSize());
    setShearX(defaultShearX());
    setShearY(defaultShearY());
    setRotation(defaultRotation());
    setAlignToPixelGrid(defaultAlignToPixelGrid());
    setAlignToPixelGridX(defaultAlignToPixelGridX());
    setAlignToPixelGridY(defaultAlignToPixelGridY());
}
