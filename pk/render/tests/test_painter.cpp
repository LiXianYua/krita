#include "PkPainter.h"
#include <cassert>
#include <vector>
#include <type_traits>
struct RecordingBackend : PkPainterBackend { std::vector<PkPaintCommand> commands; void submit(const PkPaintCommand &c) override { commands.push_back(c); } };
int main() {
 RecordingBackend b; PkPainter p(b);
 p.save(); p.setPen(PkPen(PkColor(10,20,30),2)); p.setBrush(PkBrush(PkColor(40,50,60,200)));
 p.drawRect(PkRectF(1,2,3,4)); p.restore(); assert(b.commands.size()==5);
 assert(std::holds_alternative<PkDrawRectCommand>(b.commands[3]));
 assert(p.pen().widthF()==1.0); assert(p.brush().style()==Qt::NoBrush);
 p.restore(); assert(b.commands.size()==5);
 PkTransform t(1,0,0,1,3,4); p.setTransform(t); p.save();
 PkTransform u(1,0,0,1,2,0); p.setTransform(u,true);
 assert(p.transform().dx()==5 && p.transform().dy()==4); p.restore();
 assert(p.transform().dx()==3 && p.transform().dy()==4);
 p.setRenderHint(8,true); p.setRenderHint(8,false); p.setClipRect(PkRectF(0,0,1,1),Qt::IntersectClip);
 { PkPainterPath path; path.addRect(PkRectF(0,0,2,2)); PkImage image; p.drawPath(path); p.drawImage(PkRectF(0,0,1,1),image); }
 assert(std::holds_alternative<PkDrawPathCommand>(b.commands[b.commands.size()-2]));
 assert(std::holds_alternative<PkDrawImageCommand>(b.commands.back()));
 return 0;
}
