/*
 *  SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2011 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_text_brush.h"

#include <map>

#include "kis_gbr_brush.h"
#include "kis_brushes_pipe.h"
#include <kis_dom_utils.h>
#include <PkFontRasterizer.h>


class KisTextBrushesPipe : public KisBrushesPipe<KisGbrBrush>
{
public:
    KisTextBrushesPipe() {
        m_charIndex = 0;
        m_currentBrushIndex = 0;
    }

    KisTextBrushesPipe(const KisTextBrushesPipe &rhs)
        : KisBrushesPipe<KisGbrBrush>(), // no copy here!
          m_text(rhs.m_text),
          m_charIndex(rhs.m_charIndex),
          m_currentBrushIndex(rhs.m_currentBrushIndex)
    {
        m_brushesMap.clear();

        for (const auto &entry : rhs.m_brushesMap) {
            KisGbrBrushSP brush(new KisGbrBrush(*entry.second));
            m_brushesMap.emplace(entry.first, brush);
            KisBrushesPipe<KisGbrBrush>::addBrush(brush);
        }
    }

    void setText(const PkString &text, const PkFont &font) {
        m_text = text;

        m_charIndex = 0;

        clear();

        for (int i = 0; i < m_text.length(); i++) {

            const char16_t letter = m_text.at(i);

            // skip letters that are already present in the brushes pipe
            if (m_brushesMap.find(letter) != m_brushesMap.end()) continue;

            PkImage image = renderChar(PkString(letter), font);
            KisGbrBrushSP brush(new KisGbrBrush(image, PkString(letter)));
            brush->setSpacing(0.1); // support for letter spacing?
            brush->makeMaskImage(false);

            m_brushesMap.emplace(letter, brush);
            KisBrushesPipe<KisGbrBrush>::addBrush(brush);
        }
    }

    static PkImage renderChar(const PkString& text, const PkFont &font) {
        return PkFontRasterizer::render(text, font);
    }

    void clear() override {
        m_brushesMap.clear();
        KisBrushesPipe<KisGbrBrush>::clear();
    }

    KisGbrBrushSP firstBrush() const {
        if (m_text.isEmpty() || m_brushesMap.empty()) return {};
        return m_brushesMap.at(m_text.at(0));
    }

    void notifyStrokeStarted() override {
        m_charIndex = 0;
        updateBrushIndexesImpl();
    }

    int currentBrushIndex() override {
        return m_currentBrushIndex;
    }

protected:

    int chooseNextBrush(const KisPaintInformation& info) override {
        (void)info;
        return m_currentBrushIndex;
    }

    void updateBrushIndexes(KisRandomSourceSP randomSource, int seqNo) override {
        (void)randomSource;

        if (m_text.size()) {
            m_charIndex = (seqNo >= 0 ? seqNo : (m_charIndex + 1)) % m_text.size();
        } else {
            m_charIndex = 0;
        }

        updateBrushIndexesImpl();
    }

private:
    void updateBrushIndexesImpl() {
        if (m_text.isEmpty()) return;

        if (m_charIndex >= m_text.size()) {
            m_charIndex = 0;
        }

        const char16_t letter = m_text.at(m_charIndex);
        const auto brush = m_brushesMap.find(letter);
        KIS_ASSERT_RECOVER_RETURN(brush != m_brushesMap.end());

        m_currentBrushIndex = m_brushes.indexOf(brush->second);
    }

private:
    std::map<char16_t, KisGbrBrushSP> m_brushesMap;
    PkString m_text;
    int m_charIndex;
    int m_currentBrushIndex;
};


KisTextBrush::KisTextBrush()
    : m_brushesPipe(new KisTextBrushesPipe())
{
    setPipeMode(false);
}

KisTextBrush::KisTextBrush(const KisTextBrush &rhs)
    : KisScalingSizeBrush(rhs),
      m_font(rhs.m_font),
      m_text(rhs.m_text),
      m_brushesPipe(new KisTextBrushesPipe(*rhs.m_brushesPipe))
{
}

KisTextBrush::~KisTextBrush()
{
    delete m_brushesPipe;
}

KoResourceSP KisTextBrush::clone() const
{
    return KisBrushSP(new KisTextBrush(*this));
}

bool KisTextBrush::isEphemeral() const
{
    return true;
}

bool KisTextBrush::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    (void)dev;
    (void)resourcesInterface;
    return false;
}

bool KisTextBrush::saveToDevice(PkStream *dev) const
{
    (void)dev;
    return false;
}

void KisTextBrush::setPipeMode(bool pipe)
{
    setBrushType(pipe ? PIPE_MASK : MASK);
}

bool KisTextBrush::pipeMode() const
{
    return brushType() == PIPE_MASK;
}

void KisTextBrush::setText(const PkString& txt)
{
    m_text = txt;
}

PkString KisTextBrush::text(void) const
{
    return m_text;
}

void KisTextBrush::setFont(const PkFont& font)
{
    m_font = font;
}

PkFont KisTextBrush::font() const
{
    return m_font;
}

void KisTextBrush::notifyStrokeStarted()
{
    m_brushesPipe->notifyStrokeStarted();
}

void KisTextBrush::prepareForSeqNo(const KisPaintInformation &info, int seqNo)
{
    m_brushesPipe->prepareForSeqNo(info, seqNo);
}

void KisTextBrush::generateMaskAndApplyMaskOrCreateDab(KisFixedPaintDeviceSP dst, KisBrush::ColoringInformation* coloringInformation,
    KisDabShape const& shape,
    const KisPaintInformation& info, double subPixelX, double subPixelY, qreal softnessFactor, qreal lightnessStrength) const
{
    if (brushType() == MASK) {
        KisBrush::generateMaskAndApplyMaskOrCreateDab(dst, coloringInformation, shape, info, subPixelX, subPixelY, softnessFactor, lightnessStrength);
    }
    else { /* if (brushType() == PIPE_MASK)*/
        m_brushesPipe->generateMaskAndApplyMaskOrCreateDab(dst, coloringInformation, shape, info, subPixelX, subPixelY, softnessFactor, lightnessStrength);
    }
}

KisFixedPaintDeviceSP KisTextBrush::paintDevice(const KoColorSpace * colorSpace,
    KisDabShape const& shape,
    const KisPaintInformation& info,
    double subPixelX, double subPixelY) const
{
    if (brushType() == MASK) {
        return KisBrush::paintDevice(colorSpace, shape, info, subPixelX, subPixelY);
    }
    else { /* if (brushType() == PIPE_MASK)*/
        return m_brushesPipe->paintDevice(colorSpace, shape, info, subPixelX, subPixelY);
    }
}

void KisTextBrush::toXML(PkXmlDocument& doc, PkXmlElement& e) const
{
    (void)doc;

    e.setAttribute("type", "kis_text_brush");
    e.setAttribute("spacing", KisDomUtils::toString(spacing()));
    e.setAttribute("text", m_text);
    e.setAttribute("font", m_font.toString());
    e.setAttribute("pipe", (brushType() == PIPE_MASK) ? "true" : "false");
    KisBrush::toXML(doc, e);
}

void KisTextBrush::updateBrush()
{
    KIS_ASSERT_RECOVER((brushType() == PIPE_MASK) || (brushType() == MASK)) {
        setBrushType(MASK);
    }

    if (brushType() == PIPE_MASK) {
        m_brushesPipe->setText(m_text, m_font);
        if (m_text.isEmpty()) {
            // Dummy brushtip to avoid a crash...
            setBrushTipImage(KisTextBrushesPipe::renderChar(m_text, m_font));
            return;
        }
        setBrushTipImage(m_brushesPipe->firstBrush()->brushTipImage());
    }
    else { /* if (brushType() == MASK)*/
        setBrushTipImage(KisTextBrushesPipe::renderChar(m_text, m_font));
    }

    resetOutlineCache();
    setValid(true);
}

quint32 KisTextBrush::brushIndex() const
{
    return brushType() == MASK ? 0 : 1 + m_brushesPipe->currentBrushIndex();
}

qint32 KisTextBrush::maskWidth(KisDabShape const& shape, double subPixelX, double subPixelY, const KisPaintInformation& info) const
{
    return brushType() == MASK ?
           KisBrush::maskWidth(shape, subPixelX, subPixelY, info) :
           m_brushesPipe->maskWidth(shape, subPixelX, subPixelY, info);
}

qint32 KisTextBrush::maskHeight(KisDabShape const& shape, double subPixelX, double subPixelY, const KisPaintInformation& info) const
{
    return brushType() == MASK ?
           KisBrush::maskHeight(shape, subPixelX, subPixelY, info) :
           m_brushesPipe->maskHeight(shape, subPixelX, subPixelY, info);
}

void KisTextBrush::setAngle(qreal _angle)
{
    KisBrush::setAngle(_angle);
    m_brushesPipe->setAngle(_angle);
}

void KisTextBrush::setScale(qreal _scale)
{
    KisBrush::setScale(_scale);
    m_brushesPipe->setScale(_scale);
}

void KisTextBrush::setSpacing(double _spacing)
{
    KisBrush::setSpacing(_spacing);
    m_brushesPipe->setSpacing(_spacing);
}
