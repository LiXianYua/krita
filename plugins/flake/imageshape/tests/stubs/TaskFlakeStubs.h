/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

// Task-local value-side flake contract used only by the S-09-f focused shell.
// The current main-tree KoShape painter ABI is still Qt and remains an M5
// transition.  This stub deliberately supplies a non-rendering default rather
// than inventing a Pk painter parameter that production classes would expose.

#include <PkColor.h>
#include <PkImage.h>
#include <PkList.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkSharedPointer.h>
#include <PkSize.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkTransform.h>
#include <PkVariant.h>
#include <PkXmlElement.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

inline qreal kisDegreesToRadians(qreal degrees) { return degrees * M_PI / 180.0; }
inline qreal kisRadiansToDegrees(qreal radians) { return radians * 180.0 / M_PI; }
inline qreal normalizeAngle(qreal radians)
{
    while (radians < 0.0) radians += 2.0 * M_PI;
    while (radians >= 2.0 * M_PI) radians -= 2.0 * M_PI;
    return radians;
}
inline qreal normalizeAngleDegrees(qreal degrees)
{
    while (degrees < 0.0) degrees += 360.0;
    while (degrees >= 360.0) degrees -= 360.0;
    return degrees;
}

class KoXmlWriter;
class SvgLoadingContext;
class SvgSavingContext;
class KoDocumentResourceManager;
class KoShapeLoadingContext;
class KoProperties;

namespace KoFlake { enum Position { TopLeft }; }

class KoShape
{
public:
    enum ChangeType { ContentChanged, GenericMatrixChange, ParameterChanged };

    KoShape() = default;
    KoShape(const KoShape &) = default;
    KoShape &operator=(const KoShape &) = delete;
    virtual ~KoShape() = default;
    virtual KoShape *cloneShape() const = 0;
    virtual void paint() const {}

    virtual void setSize(const PkSizeF &size) { m_size = size; }
    PkSizeF size() const { return m_size; }
    virtual void setPosition(const PkPointF &position) { m_position = position; }
    PkPointF position() const { return m_position; }
    PkPointF absolutePosition(KoFlake::Position = KoFlake::TopLeft) const { return m_position; }
    void setAbsolutePosition(const PkPointF &position, KoFlake::Position = KoFlake::TopLeft) { m_position = position; }
    PkTransform transformation() const { return m_transform; }
    void setTransformation(const PkTransform &transform) { m_transform = transform; }
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }
    void setShapeId(const PkString &id) { m_shapeId = id; }
    PkString shapeId() const { return m_shapeId; }
    void shapeChanged(ChangeType) {}
    virtual void update() { ++m_updateCount; }
    int updateCount() const { return m_updateCount; }

    template<class T> void setStroke(const PkSharedPointer<T> &) {}
    template<class T> void setBackground(const PkSharedPointer<T> &) {}

protected:
    PkTransform resizeMatrix(const PkSizeF &newSize) const
    {
        const qreal sx = m_size.width() == 0.0 ? 1.0 : newSize.width() / m_size.width();
        const qreal sy = m_size.height() == 0.0 ? 1.0 : newSize.height() / m_size.height();
        return PkTransform::fromScale(sx, sy);
    }

private:
    PkSizeF m_size{100.0, 100.0};
    PkPointF m_position;
    PkTransform m_transform;
    PkString m_shapeId;
    bool m_visible = true;
    int m_updateCount = 0;
};

class KoPathPoint
{
public:
    enum PointProperty { StartSubpath = 1, StopSubpath = 2, CloseSubpath = 4, HasControlPoint1 = 8, HasControlPoint2 = 16 };
    KoPathPoint(KoShape *parent, const PkPointF &point) : m_parent(parent), m_point(point) {}
    KoPathPoint(const KoPathPoint &other, KoShape *parent)
        : m_parent(parent), m_point(other.m_point), m_control1(other.m_control1),
          m_control2(other.m_control2), m_properties(other.m_properties) {}
    PkPointF point() const { return m_point; }
    void setPoint(const PkPointF &point) { m_point = point; }
    void setControlPoint1(const PkPointF &point) { m_control1 = point; setProperty(HasControlPoint1); }
    void setControlPoint2(const PkPointF &point) { m_control2 = point; setProperty(HasControlPoint2); }
    PkPointF controlPoint1() const { return m_control1; }
    PkPointF controlPoint2() const { return m_control2; }
    void removeControlPoint1() { unsetProperty(HasControlPoint1); }
    void removeControlPoint2() { unsetProperty(HasControlPoint2); }
    void setProperty(PointProperty property) { m_properties |= property; }
    void unsetProperty(PointProperty property) { m_properties &= ~property; }
    bool hasProperty(PointProperty property) const { return (m_properties & property) != 0; }

private:
    KoShape *m_parent = nullptr;
    PkPointF m_point;
    PkPointF m_control1;
    PkPointF m_control2;
    int m_properties = 0;
};

using KoSubpath = PkList<KoPathPoint *>;
using KoSubpathList = PkList<KoSubpath *>;

class KoParameterShape : public KoShape
{
public:
    KoParameterShape() = default;
    KoParameterShape(const KoParameterShape &other)
        : KoShape(other), m_handles(other.m_handles), m_parametric(other.m_parametric)
    {
        for (KoSubpath *path : other.m_subpaths) {
            auto *copy = new KoSubpath;
            for (KoPathPoint *point : *path) copy->append(new KoPathPoint(*point, this));
            m_subpaths.append(copy);
        }
    }
    ~KoParameterShape() override { clear(); }

    void setSize(const PkSizeF &size) override
    {
        KoShape::setSize(size);
        updatePath(size);
    }
    virtual PkPointF normalize()
    {
        if (m_subpaths.isEmpty() || m_subpaths[0]->isEmpty()) return {};
        qreal minX = m_subpaths[0]->front()->point().x();
        qreal minY = m_subpaths[0]->front()->point().y();
        qreal maxX = minX;
        qreal maxY = minY;
        for (KoSubpath *path : m_subpaths) {
            for (KoPathPoint *point : *path) {
                minX = std::min(minX, point->point().x());
                minY = std::min(minY, point->point().y());
                maxX = std::max(maxX, point->point().x());
                maxY = std::max(maxY, point->point().y());
            }
        }
        const PkPointF offset(minX, minY);
        if (minX != 0.0 || minY != 0.0) {
            for (KoSubpath *path : m_subpaths) {
                for (KoPathPoint *point : *path) point->setPoint(point->point() - offset);
            }
            setPosition(position() + offset);
        }
        KoShape::setSize(PkSizeF(maxX - minX, maxY - minY));
        return offset;
    }
    virtual PkString pathShapeId() const { return "KoPathShape"; }
    virtual void moveHandleAction(int, const PkPointF &, Qt::KeyboardModifiers = Qt::NoModifier) {}
    virtual void updatePath(const PkSizeF &) {}

    void setHandles(const PkList<PkPointF> &handles) { m_handles = handles; }
    PkList<PkPointF> handles() const { return m_handles; }
    KoSubpathList &subpaths() { return m_subpaths; }
    const KoSubpathList &subpaths() const { return m_subpaths; }
    void notifyPointsChanged() {}
    bool isParametricShape() const { return m_parametric; }
    void clear()
    {
        for (KoSubpath *path : m_subpaths) {
            for (KoPathPoint *point : *path) delete point;
            delete path;
        }
        m_subpaths.clear();
    }
    void moveTo(const PkPointF &point)
    {
        if (m_subpaths.isEmpty()) m_subpaths.append(new KoSubpath);
        m_subpaths.last()->append(new KoPathPoint(this, point));
    }
    void lineTo(const PkPointF &point) { moveTo(point); }
    void arcTo(qreal rx, qreal ry, qreal startDegrees, qreal sweepDegrees)
    {
        if (m_subpaths.isEmpty() || m_subpaths.last()->isEmpty()) return;
        const PkPointF start = m_subpaths.last()->last()->point();
        const qreal startRadians = kisDegreesToRadians(startDegrees);
        const qreal endRadians = kisDegreesToRadians(startDegrees + sweepDegrees);
        const PkPointF center(start.x() - rx * std::cos(startRadians),
                              start.y() + ry * std::sin(startRadians));
        lineTo(PkPointF(center.x() + rx * std::cos(endRadians),
                        center.y() - ry * std::sin(endRadians)));
    }

    static int arcToCurve(qreal rx, qreal ry, qreal startDegrees, qreal sweepDegrees,
                          const PkPointF &startPoint, PkPointF output[12])
    {
        const int segments = std::max(1, std::min(4, static_cast<int>(std::ceil(std::abs(sweepDegrees) / 90.0))));
        const qreal cx = startPoint.x() - rx * std::cos(startDegrees * M_PI / 180.0);
        const qreal cy = startPoint.y() + ry * std::sin(startDegrees * M_PI / 180.0);
        qreal angle = startDegrees * M_PI / 180.0;
        const qreal step = sweepDegrees * M_PI / 180.0 / segments;
        int index = 0;
        for (int segment = 0; segment < segments; ++segment) {
            const qreal next = angle + step;
            const qreal k = 4.0 / 3.0 * std::tan(step / 4.0);
            const PkPointF p0(cx + rx * std::cos(angle), cy - ry * std::sin(angle));
            const PkPointF p3(cx + rx * std::cos(next), cy - ry * std::sin(next));
            output[index++] = PkPointF(p0.x() - k * rx * std::sin(angle), p0.y() - k * ry * std::cos(angle));
            output[index++] = PkPointF(p3.x() + k * rx * std::sin(next), p3.y() + k * ry * std::cos(next));
            output[index++] = p3;
            angle = next;
        }
        return index;
    }
    PkString toString(const PkTransform &) const { return PkString(); }

private:
    KoSubpathList m_subpaths;
    PkList<PkPointF> m_handles;
    bool m_parametric = true;
};

constexpr const char *KoPathShapeId = "KoPathShape";

class KoShapeLoadingContext
{
public:
    KoShapeLoadingContext(void * = nullptr, void * = nullptr) {}
};

class KoXmlWriter
{
public:
    void startElement(const PkString &name) { m_element = name; }
    void endElement() {}
    void addAttribute(const PkString &name, const PkString &value) { m_attributes[name] = value; }
    void addAttribute(const PkString &name, const char *value) { addAttribute(name, PkString(value)); }
    void addAttribute(const PkString &name, qreal value) { addAttribute(name, PkString("%1").arg(value)); }
    PkString attribute(const PkString &name) const
    {
        auto found = m_attributes.find(name);
        return found == m_attributes.end() ? PkString() : found->second;
    }
private:
    PkString m_element;
    std::map<PkString, PkString> m_attributes;
};

class SvgSavingContext
{
public:
    KoXmlWriter &shapeWriter() { return m_writer; }
    PkString createUID(const PkString &prefix) { return prefix + "1"; }
    PkString getID(const KoShape *) { return "shape1"; }
    PkTransform userSpaceTransform() const { return {}; }
private:
    KoXmlWriter m_writer;
};

class SvgLoadingContext
{
public:
    explicit SvgLoadingContext(KoDocumentResourceManager * = nullptr) {}
    void *currentGC() const { return nullptr; }
    void *resolvedProperties() const { return nullptr; }
    PkByteArray fetchExternalFile(const PkString &) const { return m_external; }
    void setExternalFile(const PkByteArray &bytes) { m_external = bytes; }
private:
    PkByteArray m_external;
};

class SvgShape
{
public:
    virtual ~SvgShape() = default;
    virtual bool saveSvg(SvgSavingContext &) = 0;
    virtual bool loadSvg(const PkXmlElement &, SvgLoadingContext &) = 0;
};

class SvgUtil
{
public:
    class PreserveAspectRatioParser
    {
    public:
        explicit PreserveAspectRatioParser(const PkString &text) : m_text(text) {}
        PkString toString() const { return m_text; }
        bool defer = false;
    private:
        PkString m_text;
    };

    static qreal parseUnitX(void *, void *, const PkString &text) { return parse(text); }
    static qreal parseUnitY(void *, void *, const PkString &text) { return parse(text); }
    static qreal parseUnitXY(void *, void *, const PkString &text) { return parse(text); }
    static qreal parseNumber(const PkString &text) { return parse(text); }
    static void parseAspectRatio(const PreserveAspectRatioParser &, const PkRectF &,
                                 const PkRect &, PkTransform *) {}
    static void writeTransformAttributeLazy(const PkString &, const PkTransform &, KoXmlWriter &) {}
private:
    static qreal parse(const PkString &text)
    {
        const std::string value = text.PkToUtf8();
        return value.empty() ? 0.0 : std::strtod(value.c_str(), nullptr);
    }
};

class SvgStyleWriter
{
public:
    static void saveMetadata(const KoShape *, SvgSavingContext &) {}
    static void saveSvgStyle(const KoShape *, SvgSavingContext &) {}
};

namespace KisDomUtils { inline PkString toString(qreal value) { return PkString("%1").arg(value); } }

namespace KoXmlNS {
inline const PkString draw("urn:oasis:names:tc:opendocument:xmlns:drawing:1.0");
inline const PkString svg("urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0");
inline const PkString krita("http://www.calligra.org/2005/krita");
}

class KoProperties
{
public:
    void setProperty(const PkString &name, const PkVariant &value) { m_values[name] = value; }
    void setProperty(const PkString &name, int value) { m_values[name] = PkVariant(value); }
    void setProperty(const PkString &name, qreal value) { m_values[name] = PkVariant(value); }
    void setProperty(const PkString &name, bool value) { m_values[name] = PkVariant(value); }
    PkVariant value(const PkString &name) const
    {
        auto found = m_values.find(name);
        return found == m_values.end() ? PkVariant() : found->second;
    }
    bool property(const PkString &name, PkVariant &result) const
    {
        auto found = m_values.find(name);
        if (found == m_values.end()) return false;
        result = found->second;
        return true;
    }
    qreal doubleProperty(const PkString &name, qreal fallback) const { const PkVariant v = value(name); return v.isValid() ? v.toDouble() : fallback; }
    int intProperty(const PkString &name, int fallback) const { const PkVariant v = value(name); return v.isValid() ? v.toInt() : fallback; }
    bool boolProperty(const PkString &name, bool fallback) const { const PkVariant v = value(name); return v.isValid() ? v.toBool() : fallback; }
private:
    std::map<PkString, PkVariant> m_values;
};

struct KoShapeTemplate
{
    PkString id, templateId, name, family, toolTip, iconName;
    KoProperties *properties = nullptr;
};

class KoShapeFactoryBase
{
public:
    KoShapeFactoryBase(const PkString &id, const PkString &) : m_id(id) {}
    virtual ~KoShapeFactoryBase() = default;
    virtual KoShape *createDefaultShape(KoDocumentResourceManager * = nullptr) const = 0;
    virtual KoShape *createShape(const KoProperties *, KoDocumentResourceManager *manager = nullptr) const
    { return createDefaultShape(manager); }
    virtual bool supports(const PkXmlElement &, KoShapeLoadingContext &) const { return false; }
    PkString id() const { return m_id; }
    void setToolTip(const PkString &) {}
    void setFamily(const PkString &) {}
    void setLoadingPriority(int) {}
    template<class T> void setXmlElements(const T &) {}
    void setXmlElementNames(const PkString &, const PkStringList &) {}
    void addTemplate(const KoShapeTemplate &) {}
private:
    PkString m_id;
};

class KoShapeRegistry
{
public:
    static KoShapeRegistry *instance() { static KoShapeRegistry registry; return &registry; }
    void add(KoShapeFactoryBase *factory)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_factories[factory->id()] = std::unique_ptr<KoShapeFactoryBase>(factory);
    }
    int count() const { std::lock_guard<std::mutex> lock(m_mutex); return static_cast<int>(m_factories.size()); }
    bool contains(const PkString &id) const { std::lock_guard<std::mutex> lock(m_mutex); return m_factories.find(id) != m_factories.end(); }
private:
    mutable std::mutex m_mutex;
    std::map<PkString, std::unique_ptr<KoShapeFactoryBase>> m_factories;
};

class KoShapeStroke { public: explicit KoShapeStroke(qreal) {} };
class KoColorBackground { public: explicit KoColorBackground(const PkColor &) {} };
class KoShapeSavingContext {};
class KoUnit {};

class KUndo2Command
{
public:
    explicit KUndo2Command(KUndo2Command * = nullptr) {}
    virtual ~KUndo2Command() = default;
    virtual void redo() {}
    virtual void undo() {}
    virtual int id() const { return -1; }
    virtual bool mergeWith(const KUndo2Command *) { return false; }
    void setText(const PkString &) {}
};

#define kundo2_text(text) PkString(text)
namespace KisCommandUtils { enum { ChangeRectangleShapeId = 1001, ChangeEllipseShapeId = 1002 }; }

#define KIS_SAFE_ASSERT_RECOVER(condition) do { (void)sizeof(condition); } while (false)
#define KIS_ASSERT_RECOVER(condition) if (!(condition))
#define KIS_SAFE_ASSERT_RECOVER_RETURN(condition) do { if (!(condition)) return; } while (false)
#define KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(condition, value) do { if (!(condition)) return value; } while (false)
