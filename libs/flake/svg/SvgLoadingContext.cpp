/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "SvgLoadingContext.h"

#include <filesystem>
#include <cstdint>
#include <vector>
#include <pk/container/PkStack.h>
#include <PkAuxTypes.h>

#include <KoColorSpaceRegistry.h>
#include <KoColorSpaceEngine.h>
#include <KoColorProfile.h>
#include <KoDocumentResourceManager.h>

#include <FlakeDebug.h>

#include "SvgGraphicContext.h"
#include "SvgUtil.h"
#include "SvgCssHelper.h"
#include "SvgStyleParser.h"
#include "kis_debug.h"


// PkByteArray 缺 fromHex()/toHex()；color-profile 的唯一 id 往返需要
// 这两个语义，本地补两个忠实复刻（原 fromHex 跳过空白、遇非法字符停止）。
namespace {

PkByteArray pkFromHex(const PkString &hexEncoded)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(static_cast<size_t>(hexEncoded.size()) / 2 + 1);
    bool half = false;
    uint8_t current = 0;
    for (int i = 0; i < hexEncoded.size(); ++i) {
        const char16_t c = hexEncoded.at(i);
        int val = -1;
        if (c >= u'0' && c <= u'9') {
            val = c - u'0';
        } else if (c >= u'a' && c <= u'f') {
            val = c - u'a' + 10;
        } else if (c >= u'A' && c <= u'F') {
            val = c - u'A' + 10;
        }
        if (val >= 0) {
            if (half) {
                current = static_cast<uint8_t>(current | val);
                bytes.push_back(current);
                current = 0;
                half = false;
            } else {
                current = static_cast<uint8_t>(val << 4);
                half = true;
            }
        } else if (c != u' ' && c != u'\t' && c != u'\n' && c != u'\r' && c != u'\v' && c != u'\f') {
            break;
        }
    }
    return PkByteArray(bytes);
}

PkString pkToHex(const PkByteArray &ba)
{
    static const char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(static_cast<size_t>(ba.size()) * 2);
    const char *data = ba.constData();
    for (int i = 0; i < ba.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        hex.push_back(digits[c >> 4]);
        hex.push_back(digits[c & 0xF]);
    }
    return PkString(hex.c_str());
}

}

class Q_DECL_HIDDEN SvgLoadingContext::Private
{
public:
    Private()
        : zIndex(0)
        , documentResourceManager(0)
        , styleParser(0)
    {

    }

    ~Private()
    {
        if (! gcStack.isEmpty() && !gcStack.top()->isResolutionFrame) {
            // Resolution frame is usually the first and is not removed.
            warnFlake << "the context stack is not empty (current count" << gcStack.size() << ", expected 0)";
        }
        qDeleteAll(gcStack);
        gcStack.clear();
        delete styleParser;
    }
    PkStack<SvgGraphicsContext*> gcStack;
    PkString initialXmlBaseDir;
    int zIndex;
    KoDocumentResourceManager *documentResourceManager;
    PkHash<PkString, KoShape*> loadedShapes;
    PkHash<PkString, PkXmlElement> definitions;
    PkHash<PkString, const KoColorProfile*> profiles;
    SvgCssHelper cssStyles;
    SvgStyleParser *styleParser;
    FileFetcherFunc fileFetcher;
};

SvgLoadingContext::SvgLoadingContext(KoDocumentResourceManager *documentResourceManager)
    : d(new Private())
{
    d->documentResourceManager = documentResourceManager;
    d->styleParser = new SvgStyleParser(*this);
    Q_ASSERT(d->documentResourceManager);
}

SvgLoadingContext::~SvgLoadingContext()
{
}

SvgGraphicsContext *SvgLoadingContext::currentGC() const
{
    if (d->gcStack.isEmpty())
        return 0;

    return d->gcStack.top();
}

#include "parsers/SvgTransformParser.h"

SvgGraphicsContext *SvgLoadingContext::pushGraphicsContext(const PkXmlElement &element, bool inherit)
{
    SvgGraphicsContext *gc;
    // copy data from current context
    if (! d->gcStack.isEmpty() && inherit) {
        gc = new SvgGraphicsContext(*d->gcStack.top());
    } else {
        gc = new SvgGraphicsContext();
    }

    gc->textProperties = KoSvgTextProperties();

    gc->filterId = PkString(); // filters are not inherited
    gc->clipPathId = PkString(); // clip paths are not inherited
    gc->clipMaskId = PkString(); // clip masks are not inherited
    gc->display = true; // display is not inherited
    gc->opacity = 1.0; // opacity is not inherited
    gc->paintOrder = PkString(); //paint order is inherited by default

    if (!element.isNull()) {
        if (element.hasAttribute("transform")) {
            SvgTransformParser p(element.attribute("transform"));
            if (p.isValid()) {
                PkTransform mat = p.transform();
                gc->matrix = mat * gc->matrix;
            }
        }
        if (element.hasAttribute("xml:base"))
            gc->xmlBaseDir = element.attribute("xml:base");
        if (element.hasAttribute("xml:space"))
            gc->preserveWhitespace = element.attribute("xml:space") == "preserve";
    }

    d->gcStack.push(gc);

    return gc;
}

void SvgLoadingContext::popGraphicsContext()
{
    delete(d->gcStack.pop());
}

void SvgLoadingContext::setInitialXmlBaseDir(const PkString &baseDir)
{
    d->initialXmlBaseDir = baseDir;
}

PkString SvgLoadingContext::xmlBaseDir() const
{
    SvgGraphicsContext *gc = currentGC();
    return (gc && !gc->xmlBaseDir.isEmpty()) ? gc->xmlBaseDir : d->initialXmlBaseDir;
}

PkString SvgLoadingContext::absoluteFilePath(const PkString &href)
{
    std::filesystem::path info(href.PkToUtf8());
    if (! info.is_relative())
        return href;

    SvgGraphicsContext *gc = currentGC();
    if (!gc)
        return d->initialXmlBaseDir;

    PkString baseDir = d->initialXmlBaseDir;
    if (! gc->xmlBaseDir.isEmpty())
        baseDir = absoluteFilePath(gc->xmlBaseDir);

    std::filesystem::path pathInfo(baseDir.PkToUtf8());
    if (pathInfo.is_relative())
        pathInfo = std::filesystem::absolute(pathInfo);

    PkString relFile = href;
    while (relFile.startsWith(PkString("../"))) {
        relFile = relFile.mid(3);
        // 原循环 `pathInfo.setFile(pathInfo.dir(), "")` 是恒等操作：
        // 构造出的 filePath 仍为 p，不改变 pathInfo。忠实保留该结构。
    }

    PkString absFile = PkString((pathInfo.parent_path() / std::filesystem::path(relFile.PkToUtf8())).string().c_str());

    return absFile;
}

PkString SvgLoadingContext::relativeFilePath(const PkString &href)
{
    const SvgGraphicsContext *gc = currentGC();
    if (!gc) return href;

    PkString result = href;

    std::filesystem::path info(href.PkToUtf8());
    if (info.is_relative())
        return href;

    std::filesystem::path base;
    if (!gc->xmlBaseDir.isEmpty()) {
        base = std::filesystem::path(gc->xmlBaseDir.PkToUtf8());
    } else if (!d->initialXmlBaseDir.isEmpty()) {
        base = std::filesystem::path(d->initialXmlBaseDir.PkToUtf8());
    }
    if (base.is_relative())
        base = std::filesystem::absolute(base);

    std::filesystem::path rel = info.lexically_relative(base);
    if (!rel.empty()) {
        result = PkString(rel.lexically_normal().string().c_str());
    }
    // lexically_relative 返回空（根不同）时保持 href 原样 —— 对齐
    // 原 relativeFilePath 在无法计算相对路径时返回 filePath 的行为。

    return result;
}

int SvgLoadingContext::nextZIndex()
{
    return d->zIndex++;
}

void SvgLoadingContext::registerShape(const PkString &id, KoShape *shape)
{
    if (!id.isEmpty())
        d->loadedShapes.insert(id, shape);
}

KoShape* SvgLoadingContext::shapeById(const PkString &id)
{
    return d->loadedShapes.value(id);
}

void SvgLoadingContext::addDefinition(const PkXmlElement &element)
{
    const PkString id = element.attribute("id");
    if (id.isEmpty() || d->definitions.contains(id))
        return;
    d->definitions.insert(id, element);
}

PkXmlElement SvgLoadingContext::definition(const PkString &id) const
{
    return d->definitions.value(id);
}

bool SvgLoadingContext::hasDefinition(const PkString &id) const
{
    return d->definitions.contains(id);
}

void SvgLoadingContext::addStyleSheet(const PkXmlElement &styleSheet)
{
    d->cssStyles.parseStylesheet(styleSheet);
}

PkStringList SvgLoadingContext::matchingCssStyles(const PkXmlElement &element) const
{
    return d->cssStyles.matchStyles(element);
}

SvgStyleParser &SvgLoadingContext::styleParser()
{
    return *d->styleParser;
}

void SvgLoadingContext::parseProfile(const PkXmlElement &element)
{
    const PkString href = element.attribute("xlink:href");
    const PkByteArray uniqueId = pkFromHex(element.attribute("local"));
    const PkString name = element.attribute("name");

    if (element.attribute("rendering-intent", "auto") != "auto") {
        // WARNING: Krita does *not* treat rendering intents attributes of the profile!
        warnFlake << "WARNING: we do *not* treat rendering intents attributes of the profile!";
    }

    if (d->profiles.contains(name)) {
        debugFlake << "Profile already in the map!" << ppVar(name);
        return;
    }

    const KoColorProfile *profile =
            KoColorSpaceRegistry::instance()->profileByUniqueId(uniqueId);

    if (!profile && d->fileFetcher) {
        KoColorSpaceEngine *engine = KoColorSpaceEngineRegistry::instance()->get("icc");
        KIS_ASSERT(engine);
        if (engine) {
            const PkString fileName = relativeFilePath(href);
            const PkByteArray profileData = d->fileFetcher(fileName);
            if (!profileData.isEmpty()) {
                profile = engine->addProfile(profileData);

                if (profile->uniqueId() != uniqueId) {
                    warnFlake << "WARNING: ProfileID of the attached profile doesn't match the one mentioned in SVG element";
                    warnFlake << "       " << ppVar(pkToHex(profile->uniqueId()));
                    warnFlake << "       " << ppVar(pkToHex(uniqueId));
                }
            } else {
                warnFlake << "WARNING: couldn't fetch the ICCprofile file!" << fileName;
            }
        }
    }

    if (profile) {
        d->profiles.insert(name, profile);
    } else {
        warnFlake << "WARNING: couldn't load SVG profile" << ppVar(name) << ppVar(href) << ppVar(uniqueId);
    }
}

PkHash<PkString, const KoColorProfile *> SvgLoadingContext::profiles()
{
    return d->profiles;
}

KoSvgTextProperties SvgLoadingContext::resolvedProperties(bool onlyFontAndLineHeight) const
{
    KoSvgTextProperties props;
    for (auto it = d->gcStack.begin(); it != d->gcStack.end(); it++) {
        SvgGraphicsContext *gc = *it;
        KoSvgTextProperties props2 = gc->textProperties;
        props.resetNonInheritableToDefault();
        props2.inheritFrom(props, true, onlyFontAndLineHeight);
        props = props2;
    }
    return props;
}

bool SvgLoadingContext::isRootContext() const
{
    KIS_ASSERT(!d->gcStack.isEmpty());
    return d->gcStack.size() == 1;
}

void SvgLoadingContext::setFileFetcher(SvgLoadingContext::FileFetcherFunc func)
{
    d->fileFetcher = func;
}

PkByteArray SvgLoadingContext::fetchExternalFile(const PkString &url)
{
    return d->fileFetcher ? d->fileFetcher(url) : PkByteArray();
}
