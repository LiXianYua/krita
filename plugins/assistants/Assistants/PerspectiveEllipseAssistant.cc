/*
 * SPDX-FileCopyrightText: 2022 Srirupa Datta <srirupa.sps@gmail.com>
 */

#include "PerspectiveEllipseAssistant.h"
#include "PerspectiveBasedAssistantHelper.h"


#include <PkTransform.h>

#include "kis_algebra_2d.h"
#include <Eigen/Eigenvalues>

#include <cmath>

#include <functional>


// ################################## Ellipse in Polygon (in Perspective) #######################################

class EllipseInPolygon
{
public:

    EllipseInPolygon();

    // nomenclature:
    // "final ellipse" - ellipse that the user wants
    // "rerotated ellipse" - "final ellipse" that has been rotated (transformed) to have axes parallel to X and Y axes
    //    (called "rerotated" because now the rotation angle is 0)
    // "canonical ellipse" - "final ellipse" that has been rotated to have axes parallel to X and Y axes,
    //    *and* moved so that the center is in point (0, 0)
    // --- every "ellipse" above also means "coordination system for that ellipse"

    // "ellipse formula" - ax^2 + bxy + cy^2 + dx + ey + f = 0
    // "vertices" - points on axes

    // functions

    ///
    /// \brief updateToPolygon
    /// This function makes all the necessary calculations and updates all data, not just the polygon
    ///  according to the polygon that was provided as the parameter
    /// \param polygon polygon that contains the ellipse
    /// \returns whether the ellipse is valid or not
    ///
    bool updateToPolygon(PkVector<PkPointF> _polygon);

    ///
    /// \brief setSimpleEllipseVertices sets vertices of this ellipse to the "simple ellipse" class
    /// to be drawn and used
    /// \param ellipse
    /// \return
    ///
    bool setSimpleEllipseVertices(Ellipse& ellipse) const;

    bool isValid() const { return m_valid; }

    ///
    /// \brief formulaRepresentsAnEllipse
    /// parameters are first three coefficients from a formula: ax^2 + bxy + cy^2 + dx + ey + f = 0
    /// \param a - first coefficient
    /// \param b - second coefficient
    /// \param c - third coefficient
    /// \return true if the formula represents an ellipse, otherwise false
    ///
    static bool formulaRepresentsAnEllipse(double a, double b, double c);

    // unused for now; will be used to move the ellipse towards any vanishing point
    // might need more info about vanishing points (for example, might need all points)
    // moveTowards(PkPointF vanishingPoint, PkPointF cursorStartPoint, PkPointF cursorEndPoint);

    // ----- data -----
    // keep the known-size-vectors the same size!

    PkVector<PkPointF> polygon;
    PkTransform originalTransform; // original square-to-polygon transform, including perspective


    PkVector<double> finalFormula; // final ellipse formula using ax^2 + bxy + cy^2 + dx + ey + f = 0
    PkVector<double> rerotatedFormula; // rerotated ellipse formula using ax^2 + bxy + cy^2 + dx + ey + f = 0

    double finalAxisAngle {0.0}; // theta - angle of the final ellipse's X axis
    double finalAxisReverseAngleCos {0.0}; // cos(-theta) -> used for calculating rerotatedFormula
    double finalAxisReverseAngleSin {0.0}; // sin(-theta) -> used for calculating rerotatedFormula

    PkVector<double> finalEllipseCenter; // always just two values; PkPointF could have too low of a precision for calculations

    double axisXLength {0.0}; // all "final", "rerotated" and "canonical" ellipses have the same axes lengths
    double axisYLength {0.0};

    PkVector<PkPointF> finalVertices; // used to draw ellipses and project the cursor points

protected:

    void setFormula(PkVector<double>& formula, double a, double b, double c, double d, double e, double f);
    void setPoint(PkVector<double>& point, double x, double y);


    bool m_valid {false};

};


EllipseInPolygon::EllipseInPolygon()
{
    finalFormula.clear();
    rerotatedFormula.clear();
    finalFormula << 1 << 0 << 1 << 0 << 0 << 0;
    rerotatedFormula << 1 << 0 << 1 << 0 << 0 << 0;

    finalEllipseCenter.clear();
    finalEllipseCenter << 0 << 0;

    finalVertices.clear();
    finalVertices << PkPointF(-1, 0) << PkPointF(1, 0) << PkPointF(0, 1);
}

bool EllipseInPolygon::updateToPolygon(PkVector<PkPointF> _polygon)
{
    PkTransform transform;

    m_valid = false; // let's make it false in case we return in the middle of the work
    polygon = _polygon; // the assistant needs to know the polygon even when it doesn't allow for a correct ellipse

    // this calculates the perspective transform that represents the current quad (polygon)
    // this is the transform that changes the original (0, 0, 1, 1) square to the quad
    // that means that our "original" ellipse is the circle in that square (with center in (0.5, 0.5), and radius 0.5)
    if (!PkTransform::squareToQuad(polygon, transform)) {
        return false;
    }

    originalTransform = transform;

    // using the perspective transform, we can calculate some points on the ellipse
    // any points from the original ellipse would work here
    // but pt1-4 are just the simplest ones to write
    // and pR is another one easy to calculate (common point between the original ellipse and a line `y = x`)
    PkPointF pt1 = originalTransform.map(PkPointF(0.5, 1.0));
    PkPointF pt2 = originalTransform.map(PkPointF(1.0, 0.5));
    PkPointF pt3 = originalTransform.map(PkPointF(0.5, 0.0));
    PkPointF pt4 = originalTransform.map(PkPointF(0.0, 0.5));
    // a point on the ellipse and on the `y = x` line
    PkPointF ptR = originalTransform.map(PkPointF(0.5 - 1/(2*sqrt(2)), 0.5 - 1/(2*sqrt(2))));


    // using the points from above (pt1-4 and ptR) we can construct a linear equation for the final ellipse formula
    // the general ellipse formula is: `ax^2 + bxy + cy^2 + dx + ey + f = 0`
    // but since a cannot ever be 0, we can temporarily reduce the formula to be `x^2 + Bxy + Cy^2 + Dx + Ey + F = 0`
    // where B = b/a etc.
    Eigen::MatrixXd A(5, 5);
    A <<          ptR.x() * ptR.y(), ptR.y() * ptR.y(), ptR.x(), ptR.y(), 1.0,
                  pt1.x() * pt1.y(), pt1.y() * pt1.y(), pt1.x(), pt1.y(), 1.0,
                  pt2.x() * pt2.y(), pt2.y() * pt2.y(), pt2.x(), pt2.y(), 1.0,
                  pt3.x() * pt3.y(), pt3.y() * pt3.y(), pt3.x(), pt3.y(), 1.0,
                  pt4.x() * pt4.y(), pt4.y() * pt4.y(), pt4.x(), pt4.y(), 1.0;

    Eigen::VectorXd bVector(5);
    bVector << - ptR.x() * ptR.x(), - pt1.x() * pt1.x(), - pt2.x() * pt2.x(),  - pt3.x() * pt3.x(), - pt4.x() * pt4.x();

    Eigen::VectorXd xSolution = A.fullPivLu().solve(bVector);

    // generic ellipse formula coefficients for the final formula
    // assigned to new variables to better see the calculations
    // (even with "x" as a solution vector variable, it would be difficult to spot error when everything looks like x(2)*x(4)/x(3)*x(1) etc.)
    qreal a = 1;
    qreal b = xSolution(0);

    qreal c = xSolution(1);
    qreal d = xSolution(2);

    qreal e = xSolution(3);
    qreal f = xSolution(4);

    // check if this is an ellipse
    if (!formulaRepresentsAnEllipse(a, b, c)) {
        return false;
    }

    setFormula(finalFormula, a, b, c, d, e, f);

    // x = (be - 2cd)/(4c - b^2)
    // y = (bd - 2e)/(4c - b^2)
    finalEllipseCenter.clear();
    finalEllipseCenter << ((double)b*e - 2*c*d)/(4*c - b*b) << ((double)b*d - 2*e)/(4*c - b*b);
    finalAxisAngle = std::atan2(b, a - c)/2;

    // use finalAxisAngle to find the cos and sin
    // and replace the final coordinate system with the rerotated one
    qreal K = std::cos(-finalAxisAngle);
    qreal L = std::sin(-finalAxisAngle);

    // this allows to calculate the formula for the rerotated ellipse
    qreal aprim = K*K*a - K*L*b + L*L*c;
    qreal bprim = 2*K*L*a + K*K*b - L*L*b - 2*K*L*c;
    qreal cprim = L*L*a + K*L*b + K*K*c;
    qreal dprim = K*d - L*e;
    qreal eprim = L*d + K*e;
    qreal fprim = f;

    if (!formulaRepresentsAnEllipse(aprim, bprim, cprim)) {
        return false;
    }

    finalAxisReverseAngleCos = K;
    finalAxisReverseAngleSin = L;

    setFormula(rerotatedFormula, aprim, bprim, cprim, dprim, eprim, fprim);

    // third attempt at new center:
    // K' = K
    // L' = -L
    // note that this will be in a different place, because the ellipse wasn't moved to have center in (0, 0), but still rotate around point (0,0)
    // and any point that is not (0, 0), when rotated around (0, 0) with an angle that isn't 0, 360 etc. degrees, will end up in a different place
    PkPointF rerotatedCenter = PkPointF(K*finalEllipseCenter[0] - L*finalEllipseCenter[1], K*finalEllipseCenter[1] + L*finalEllipseCenter[0]);

    qreal rx = std::sqrt(std::pow(rerotatedCenter.x(), 2) + std::pow(rerotatedCenter.y(), 2)*cprim/aprim - fprim/aprim);
    qreal ry = sqrt(rx*rx*aprim/cprim);

    axisXLength = rx;
    axisYLength = ry;

#if 0 // debug
    // they should be very close to cprim, dprim etc., when multiplied by aprim (since this only gives us a formula where aprim_recreated would be equal to 1)
    qreal cprim_recreated = (rx*rx)/(ry*ry);
    qreal dprim_recreated = -2*rerotatedCenter.x();
    qreal eprim_recreated = -2*rerotatedCenter.y()*(rx*rx)/(ry*ry);
    qreal fprim_recreated = std::pow(rerotatedCenter.x(), 2) + std::pow(rerotatedCenter.y(), 2)*(rx*rx)/(ry*ry) - (rx*rx);

    if (debug) qCritical() << "recreated equation (with 1): " << 1 << 0 << cprim_recreated << dprim_recreated << eprim_recreated << fprim_recreated;
    if (debug) qCritical() << "recreated equation: (actual)" << aprim << 0 << aprim*cprim_recreated << aprim*dprim_recreated << aprim*eprim_recreated << aprim*fprim_recreated;

    qreal eps = 0.00001;
    auto fuzzyCompareWithEps = [eps] (qreal a, qreal b) { return abs(a - b) < eps; };

    KIS_SAFE_ASSERT_RECOVER_NOOP(fuzzyCompareWithEps(aprim*cprim_recreated, cprim));
    KIS_SAFE_ASSERT_RECOVER_NOOP(fuzzyCompareWithEps(aprim*dprim_recreated, dprim));
    KIS_SAFE_ASSERT_RECOVER_NOOP(fuzzyCompareWithEps(aprim*eprim_recreated, eprim));
    KIS_SAFE_ASSERT_RECOVER_NOOP(fuzzyCompareWithEps(aprim*fprim_recreated, fprim));

#endif

    auto convertToPreviousCoordsSystem = [K, L] (PkPointF p) { return PkPointF(K*p.x() + L*p.y(), K*p.y() - L*p.x()); };

    // they most probably don't need a higher precision than float
    // (though they are used to calculate the brush position...)
    PkPointF leftVertexRerotated = rerotatedCenter + PkPointF(-rx, 0);
    PkPointF rightVertedRerotated = rerotatedCenter + PkPointF(rx, 0);
    PkPointF topVertedRerotated = rerotatedCenter + PkPointF(0, ry);

    PkPointF leftVertexFinal = convertToPreviousCoordsSystem(leftVertexRerotated);
    PkPointF rightVertexFinal = convertToPreviousCoordsSystem(rightVertedRerotated);
    PkPointF topVertexFinal = convertToPreviousCoordsSystem(topVertedRerotated);

    PkVector<PkPointF> result;
    result << leftVertexFinal << rightVertexFinal << topVertexFinal;

    finalVertices = result;

    m_valid = true;
    return true;
}

bool EllipseInPolygon::setSimpleEllipseVertices(Ellipse &ellipse) const
{
    if (finalVertices.size() > 2) {
        return ellipse.set(finalVertices[0], finalVertices[1], finalVertices[2]);
    }
    return false;
}

bool EllipseInPolygon::formulaRepresentsAnEllipse(double a, double b, double c)
{
    return (b*b - 4*a*c) < 0;
}

void EllipseInPolygon::setFormula(PkVector<double> &formula, double a, double b, double c, double d, double e, double f)
{
    if (formula.size() != 6) {
        formula.clear();
        formula << a << b << c << d << e << f;
    } else {
        formula[0] = a;
        formula[1] = b;
        formula[2] = c;
        formula[3] = d;
        formula[4] = e;
        formula[5] = f;
    }
}

void EllipseInPolygon::setPoint(PkVector<double> &point, double x, double y)
{
    if (point.size() != 2) {
        point.clear();
        point << x << y;
    } else {
        point[0] = x;
        point[1] = y;
    }
}


// ################################## Perspective Ellipse Assistant #######################################


class PerspectiveEllipseAssistant::Private
{
public:
    EllipseInPolygon ellipseInPolygon;
    Ellipse simpleEllipse;

    bool cacheValid { false };

    PerspectiveBasedAssistantHelper::CacheData cache;

    PkVector<PkPointF> cachedPoints; // points on the polygon

};

PerspectiveEllipseAssistant::PerspectiveEllipseAssistant()
    : KisPaintingAssistant("perspective ellipse", PkString("Perspective Ellipse assistant"))
    , d(new Private())
{

}

PerspectiveEllipseAssistant::~PerspectiveEllipseAssistant() {}

PerspectiveEllipseAssistant::PerspectiveEllipseAssistant(const PerspectiveEllipseAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , d(new Private())
{
    updateCache();
}

KisPaintingAssistantSP PerspectiveEllipseAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new PerspectiveEllipseAssistant(*this, handleMap));
}

PkPointF PerspectiveEllipseAssistant::project(const PkPointF& pt, const PkPointF& strokeBegin)
{
    (void)strokeBegin;
    assert(isAssistantComplete());

    updateCache();
    if (!d->cacheValid || !d->ellipseInPolygon.isValid() ||
        !d->ellipseInPolygon.setSimpleEllipseVertices(d->simpleEllipse)) {
        return pt;
    }

    return d->simpleEllipse.project(pt);
}

PkPointF PerspectiveEllipseAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool /*snapToAny*/, qreal /*moveThresholdPt*/)
{
    return project(pt, strokeBegin);
}

void PerspectiveEllipseAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = PkPointF();
    strokeBegin = PkPointF();
}




PkRect PerspectiveEllipseAssistant::boundingRect() const
{
     if (!isAssistantComplete()) {
       return KisPaintingAssistant::boundingRect();
    }

    updateCache();
    if (d->cacheValid && d->ellipseInPolygon.isValid() &&
        d->ellipseInPolygon.setSimpleEllipseVertices(d->simpleEllipse)) {
       return d->simpleEllipse.boundingRect().adjusted(-2, -2, 2, 2).toAlignedRect();
    } else {
       return PkRect();
    }
}

PkPointF PerspectiveEllipseAssistant::getDefaultEditorPosition() const
{
    PkPointF centroid(0, 0);
    for (int i = 0; i < 4; ++i) {
        centroid += *handles()[i];
    }

    return centroid * 0.25;
}

bool PerspectiveEllipseAssistant::isEllipseValid()
{
    updateCache();
    return isAssistantComplete() && d->cacheValid && d->ellipseInPolygon.isValid();
}

void PerspectiveEllipseAssistant::updateCache() const
{
    if (!isAssistantComplete()) {
        d->cacheValid = false;
        return;
    }

    // handles -> points -> polygon
    // check the cached points, whether they are the same as handles
    if (d->cachedPoints.size() == handles().size()) {
        for (int i = 0; i < handles().size(); ++i) {
            if (d->cachedPoints[i] != *handles()[i]) break;
            if (i == handles().size() - 1) {
                // The current snapshot has already been rebuilt. Preserve
                // whether that rebuild produced valid geometry.
                return;
            }
        }
    }

    d->cacheValid = false;

    d->cachedPoints = PkVector<PkPointF>();
    for (int i = 0; i < handles().size(); ++i) {
        d->cachedPoints << *handles()[i];
    }

    d->ellipseInPolygon = EllipseInPolygon();
    d->simpleEllipse = Ellipse();
    d->cache = PerspectiveBasedAssistantHelper::CacheData();


    PkPolygonF poly = PkPolygonF(d->cachedPoints);

    if (!PerspectiveBasedAssistantHelper::getTetragon(handles(), isAssistantComplete(), poly)) { // this function changes poly to some "standardized" version, or a triangle when it cannot be achieved
        return;
    }

    if (!d->ellipseInPolygon.updateToPolygon(poly) ||
        !d->ellipseInPolygon.setSimpleEllipseVertices(d->simpleEllipse)) {
        return;
    }

    PerspectiveBasedAssistantHelper::updateCacheData(d->cache, poly);
    d->cacheValid = true;

}

bool PerspectiveEllipseAssistant::isAssistantComplete() const
{   
    return handles().size() >= 4;
}

bool PerspectiveEllipseAssistant::contains(const PkPointF &point) const
{

    PkPolygonF poly;
    if (!PerspectiveBasedAssistantHelper::getTetragon(handles(), isAssistantComplete(), poly)) return false;
    return poly.containsPoint(point, Qt::OddEvenFill);
}

qreal PerspectiveEllipseAssistant::distance(const PkPointF &point) const
{
    updateCache();
    if (!d->cacheValid || !d->ellipseInPolygon.isValid()) {
        return 1.0;
    }
    return PerspectiveBasedAssistantHelper::distanceInGrid(d->cache, point);
}

bool PerspectiveEllipseAssistant::isActive() const
{
    return isSnappingActive();
}

PerspectiveEllipseAssistantFactory::PerspectiveEllipseAssistantFactory()
{
}

PerspectiveEllipseAssistantFactory::~PerspectiveEllipseAssistantFactory()
{
}

PkString PerspectiveEllipseAssistantFactory::id() const
{
    return "perspective ellipse";
}

PkString PerspectiveEllipseAssistantFactory::name() const
{
    return PkString("Perspective Ellipse");
}

KisPaintingAssistant* PerspectiveEllipseAssistantFactory::createPaintingAssistant() const
{
    return new PerspectiveEllipseAssistant;
}
