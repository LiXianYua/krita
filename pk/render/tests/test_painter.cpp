#include "painter_case.h"
#include "PkPainter.h"

#include <cstdint>
#include <vector>

#include "pk_binder_painter_case.inc"

namespace {

struct RecordingBackend final : PkPainterBackend
{
    std::vector<PkPaintCommand> commands;

    void submit(const PkPaintCommand &command) override
    {
        commands.push_back(command);
    }
};

#define PK_CHECK_ONE_COMMAND(backend, call)                                  \
    do {                                                                      \
        const std::size_t beforeCommandCount_ = (backend).commands.size();    \
        call;                                                                 \
        PK_COMPARE((backend).commands.size(), beforeCommandCount_ + 1);       \
    } while (false)

void compareColor(const PkColor &actual, int red, int green, int blue, int alpha = 255)
{
    PK_COMPARE(actual.red(), red);
    PK_COMPARE(actual.green(), green);
    PK_COMPARE(actual.blue(), blue);
    PK_COMPARE(actual.alpha(), alpha);
}

void compareTransform(const PkTransform &actual,
                      qreal m11, qreal m12, qreal m21, qreal m22,
                      qreal dx, qreal dy)
{
    PK_COMPARE(actual.m11(), m11);
    PK_COMPARE(actual.m12(), m12);
    PK_COMPARE(actual.m21(), m21);
    PK_COMPARE(actual.m22(), m22);
    PK_COMPARE(actual.dx(), dx);
    PK_COMPARE(actual.dy(), dy);
}

} // namespace

void PkPainterCase::commandOrderAndPayloads()
{
    RecordingBackend backend;
    PkPainter painter(backend);
    const PkPen pen(PkColor(10, 20, 30), 2.5);
    const PkBrush brush(PkColor(40, 50, 60, 200));
    const PkTransform transform(2, 3, 4, 5, 6, 7);
    const PkRectF clip(1, 2, 30, 40);
    const PkLineF line(PkPointF(3, 4), PkPointF(5, 6));
    const PkRectF rect(7, 8, 9, 10);
    const PkRectF ellipse(11, 12, 13, 14);
    const PkRectF arc(15, 16, 17, 18);
    PkPainterPath path;
    path.addRect(PkRectF(19, 20, 21, 22));
    const PkPolygonF polygon(PkVector<PkPointF>{PkPointF(1, 2), PkPointF(3, 4), PkPointF(5, 6)});
    PkImage image(2, 1, PkImage::Format_ARGB32);
    image.setPixel(0, 0, UINT32_C(0xff123456));

    PK_CHECK_ONE_COMMAND(backend, painter.save());
    PK_CHECK_ONE_COMMAND(backend, painter.setPen(pen));
    PK_CHECK_ONE_COMMAND(backend, painter.setBrush(brush));
    PK_CHECK_ONE_COMMAND(backend, painter.setTransform(transform));
    PK_CHECK_ONE_COMMAND(backend, painter.setRenderHint(0x20u, false));
    PK_CHECK_ONE_COMMAND(backend, painter.setClipRect(clip, Qt::IntersectClip));
    PK_CHECK_ONE_COMMAND(backend, painter.drawLine(line));
    PK_CHECK_ONE_COMMAND(backend, painter.drawRect(rect));
    PK_CHECK_ONE_COMMAND(backend, painter.drawEllipse(ellipse));
    PK_CHECK_ONE_COMMAND(backend, painter.drawArc(arc, 32, -48));
    PK_CHECK_ONE_COMMAND(backend, painter.drawPath(path));
    PK_CHECK_ONE_COMMAND(backend, painter.drawPolygon(polygon));
    PK_CHECK_ONE_COMMAND(backend, painter.drawImage(PkRectF(23, 24, 25, 26), image));
    PK_CHECK_ONE_COMMAND(backend, painter.restore());

    PK_COMPARE(backend.commands.size(), std::size_t(14));
    PK_VERIFY(std::holds_alternative<PkSaveCommand>(backend.commands[0]));
    const auto &penCommand = std::get<PkSetPenCommand>(backend.commands[1]);
    compareColor(penCommand.pen.color(), 10, 20, 30);
    PK_COMPARE(penCommand.pen.widthF(), 2.5);
    const auto &brushCommand = std::get<PkSetBrushCommand>(backend.commands[2]);
    compareColor(brushCommand.brush.color(), 40, 50, 60, 200);
    PK_COMPARE(brushCommand.brush.style(), Qt::SolidPattern);
    const auto &transformCommand = std::get<PkSetTransformCommand>(backend.commands[3]);
    PK_VERIFY(transformCommand.transform == transform);
    PK_VERIFY(!transformCommand.combine);
    const auto &hintCommand = std::get<PkSetRenderHintCommand>(backend.commands[4]);
    PK_COMPARE(hintCommand.hint, 0x20u);
    PK_VERIFY(!hintCommand.enabled);
    const auto &clipCommand = std::get<PkSetClipRectCommand>(backend.commands[5]);
    PK_COMPARE(clipCommand.rect, clip);
    PK_COMPARE(clipCommand.operation, Qt::IntersectClip);
    PK_COMPARE(std::get<PkDrawLineCommand>(backend.commands[6]).line, line);
    PK_COMPARE(std::get<PkDrawRectCommand>(backend.commands[7]).rect, rect);
    PK_COMPARE(std::get<PkDrawEllipseCommand>(backend.commands[8]).rect, ellipse);
    const auto &arcCommand = std::get<PkDrawArcCommand>(backend.commands[9]);
    PK_COMPARE(arcCommand.rect, arc);
    PK_COMPARE(arcCommand.startAngle16, 32);
    PK_COMPARE(arcCommand.spanAngle16, -48);
    PK_COMPARE(std::get<PkDrawPathCommand>(backend.commands[10]).path, path);
    PK_COMPARE(std::get<PkDrawPolygonCommand>(backend.commands[11]).polygon, polygon);
    const auto &imageCommand = std::get<PkDrawImageCommand>(backend.commands[12]);
    PK_COMPARE(imageCommand.target, PkRectF(23, 24, 25, 26));
    PK_COMPARE(imageCommand.image.pixel(0, 0), UINT32_C(0xff123456));
    PK_VERIFY(std::holds_alternative<PkRestoreCommand>(backend.commands[13]));
}

void PkPainterCase::nonCommutingCombinedTransform()
{
    RecordingBackend backend;
    PkPainter painter(backend);
    PkTransform current;
    current.translate(10, 20);
    PkTransform scale;
    scale.scale(2, 3);

    painter.setTransform(current);
    const std::size_t before = backend.commands.size();
    painter.setTransform(scale, true);

    PK_COMPARE(backend.commands.size(), before + 1);
    compareTransform(painter.transform(), 2, 0, 0, 3, 10, 20);
    const auto &command = std::get<PkSetTransformCommand>(backend.commands.back());
    PK_VERIFY(command.transform == scale);
    PK_VERIFY(command.combine);
}

void PkPainterCase::styleOverloadsConstructFreshValues()
{
    RecordingBackend backend;
    PkPainter painter(backend);
    PkPen oldPen(PkBrush(PkColor(90, 80, 70)), 8.5);
    oldPen.setStyle(Qt::DashLine);
    oldPen.setCapStyle(Qt::RoundCap);
    oldPen.setDashPattern({2.0, 4.0});
    oldPen.setCosmetic(true);
    painter.setPen(oldPen);
    const std::size_t penBefore = backend.commands.size();
    painter.setPen(Qt::DotLine);

    PK_COMPARE(backend.commands.size(), penBefore + 1);
    const PkPen resetPen = painter.pen();
    compareColor(resetPen.color(), 0, 0, 0);
    PK_COMPARE(resetPen.widthF(), 1.0);
    PK_COMPARE(resetPen.style(), Qt::DotLine);
    PK_COMPARE(resetPen.capStyle(), Qt::SquareCap);
    PK_VERIFY(resetPen.dashPattern().empty());
    PK_VERIFY(!resetPen.isCosmetic());
    const auto &penCommand = std::get<PkSetPenCommand>(backend.commands.back());
    PK_COMPARE(penCommand.pen.style(), Qt::DotLine);
    PK_COMPARE(penCommand.pen.widthF(), 1.0);

    painter.setBrush(PkBrush(PkColor(12, 34, 56)));
    const std::size_t brushBefore = backend.commands.size();
    painter.setBrush(Qt::Dense3Pattern);

    PK_COMPARE(backend.commands.size(), brushBefore + 1);
    compareColor(painter.brush().color(), 0, 0, 0);
    PK_COMPARE(painter.brush().style(), Qt::Dense3Pattern);
    const auto &brushCommand = std::get<PkSetBrushCommand>(backend.commands.back());
    compareColor(brushCommand.brush.color(), 0, 0, 0);
    PK_COMPARE(brushCommand.brush.style(), Qt::Dense3Pattern);
}

void PkPainterCase::exactMeasuredConsumerSpellings()
{
    static_assert(int(PkPainter::RenderHint::Antialiasing) == 0x01,
                  "Qt 5.15 nested render-hint value must be preserved");
    static_assert(int(PkPainter::Antialiasing) == 0x01,
                  "Qt 5.15 direct render-hint value must be preserved");

    const PkPen solidPen(Qt::SolidLine);
    compareColor(solidPen.color(), 0, 0, 0);
    PK_COMPARE(solidPen.widthF(), 1.0);
    PK_COMPARE(solidPen.style(), Qt::SolidLine);
    PK_COMPARE(solidPen.capStyle(), Qt::SquareCap);
    PK_VERIFY(solidPen.dashPattern().empty());
    PK_VERIFY(!solidPen.isCosmetic());

    RecordingBackend backend;
    PkPainter painter(backend);
    painter.setRenderHint(PkPainter::RenderHint::Antialiasing);
    painter.setRenderHints(PkPainter::Antialiasing, false);

    PK_COMPARE(backend.commands.size(), std::size_t(2));
    const auto &nested = std::get<PkSetRenderHintCommand>(backend.commands[0]);
    PK_COMPARE(nested.hint, 0x01u);
    PK_VERIFY(nested.enabled);
    const auto &direct = std::get<PkSetRenderHintCommand>(backend.commands[1]);
    PK_COMPARE(direct.hint, 0x01u);
    PK_VERIFY(!direct.enabled);
}

void PkPainterCase::penRetainsBrushAndRoundsWidth()
{
    const PkBrush denseBrush(PkColor(21, 43, 65));
    PkBrush styledBrush = denseBrush;
    styledBrush.setStyle(Qt::Dense5Pattern);
    PkPen pen(styledBrush, 1.6);

    compareColor(pen.color(), 21, 43, 65);
    PK_COMPARE(pen.brush().style(), Qt::Dense5Pattern);
    PK_COMPARE(pen.width(), 2);

    pen.setWidthF(-1.6);
    PK_COMPARE(pen.width(), -2);
    pen.setColor(PkColor(11, 22, 33));
    compareColor(pen.brush().color(), 11, 22, 33);
    PK_COMPARE(pen.brush().style(), Qt::Dense5Pattern);
}

void PkPainterCase::saveRestoreAndEmptyRestore()
{
    RecordingBackend backend;
    PkPainter painter(backend);
    painter.setPen(PkPen(PkColor(1, 2, 3), 4));
    painter.setBrush(PkBrush(PkColor(5, 6, 7)));
    PkTransform original;
    original.translate(8, 9);
    painter.setTransform(original);
    painter.setRenderHint(0x40u, true);
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::NoBrush);
    painter.setTransform(PkTransform());
    painter.setRenderHint(0x40u, false);
    const std::size_t beforeRestore = backend.commands.size();
    painter.restore();

    PK_COMPARE(backend.commands.size(), beforeRestore + 1);
    compareColor(painter.pen().color(), 1, 2, 3);
    PK_COMPARE(painter.pen().widthF(), 4.0);
    compareColor(painter.brush().color(), 5, 6, 7);
    PK_VERIFY(painter.transform() == original);

    const std::size_t beforeEmptyRestore = backend.commands.size();
    painter.restore();
    PK_COMPARE(backend.commands.size(), beforeEmptyRestore);
    PK_VERIFY(painter.transform() == original);
}

void PkPainterCase::copiedPathAndImagePayloadsOutliveSources()
{
    RecordingBackend backend;
    PkPainter painter(backend);
    {
        PkPainterPath path;
        path.moveTo(1, 2);
        path.lineTo(3, 4);
        PkImage image(2, 1, PkImage::Format_ARGB32);
        image.setPixel(0, 0, UINT32_C(0xff102030));
        image.setPixel(1, 0, UINT32_C(0xff405060));
        painter.drawPath(path);
        painter.drawImage(PkRectF(5, 6, 7, 8), image);

        path.setElementPositionAt(1, 99, 100);
        image.setPixel(0, 0, UINT32_C(0xffabcdef));
    }

    const auto &recordedPath = std::get<PkDrawPathCommand>(backend.commands[0]).path;
    PK_COMPARE(recordedPath.elementCount(), 2);
    PK_COMPARE(recordedPath.elementAt(0).x, 1.0);
    PK_COMPARE(recordedPath.elementAt(1).x, 3.0);
    PK_COMPARE(recordedPath.elementAt(1).y, 4.0);
    const auto &recordedImage = std::get<PkDrawImageCommand>(backend.commands[1]).image;
    PK_VERIFY(!recordedImage.isNull());
    PK_COMPARE(recordedImage.width(), 2);
    PK_COMPARE(recordedImage.pixel(0, 0), UINT32_C(0xff102030));
    PK_COMPARE(recordedImage.pixel(1, 0), UINT32_C(0xff405060));
}

void PkPainterCase::measuredOverloadsSubmitOneCommand()
{
    RecordingBackend backend;
    PkPainter painter(backend);
    auto oneMore = [&](std::size_t before) { PK_COMPARE(backend.commands.size(), before + 1); };

    std::size_t before = backend.commands.size();
    painter.setPen(PkColor(1, 2, 3));
    oneMore(before);
    PK_VERIFY(std::holds_alternative<PkSetPenCommand>(backend.commands.back()));
    before = backend.commands.size();
    painter.setBrush(PkColor(4, 5, 6));
    oneMore(before);
    PK_VERIFY(std::holds_alternative<PkSetBrushCommand>(backend.commands.back()));
    before = backend.commands.size();
    painter.setRenderHints(0x80u, true);
    oneMore(before);
    PK_VERIFY(std::holds_alternative<PkSetRenderHintCommand>(backend.commands.back()));
    before = backend.commands.size();
    painter.drawLine(PkPointF(7, 8), PkPointF(9, 10));
    oneMore(before);
    PK_COMPARE(std::get<PkDrawLineCommand>(backend.commands.back()).line,
               PkLineF(PkPointF(7, 8), PkPointF(9, 10)));
    before = backend.commands.size();
    painter.drawEllipse(PkPointF(20, 30), 4, 5);
    oneMore(before);
    PK_COMPARE(std::get<PkDrawEllipseCommand>(backend.commands.back()).rect,
               PkRectF(16, 25, 8, 10));
}

PK_TEST_APPLESS_MAIN(PkPainterCase)
