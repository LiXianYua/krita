// R-25 Task 2 判据②：候选 PkStream* 形状 —— libs/image/tests/
// kis_liquify_transform_worker_test.cpp:23-32 (getWorkerFromIODeviceXml) 的
// 形状对齐试探。
//
// 计划要求第一步先确认 getWorkerFromIODeviceXml(QIODevice*) 是不是死代码——
// 已现场确认不是：`testMaskRendering()`（真正的 `private slots` 测试方法，
// `SIMPLE_TEST_MAIN` 会把它跑起来）调用 `getWorkerFromXml()`，后者调用
// `getWorkerFromIODeviceXml(&zipFile)`/`getWorkerFromIODeviceXml(&file)`——
// 调用链是活的，不是只定义没引用的死代码。
//
// 但整份 kis_liquify_transform_worker_test.cpp 直接 -fsyntax-only 试接撞墙：
//   $ g++ -std=c++17 -fsyntax-only -I pk/xml -I pk/xml/compat -I pk/string \
//       -I pk/string/compat -I pk/port -I pk/port/compat \
//       libs/image/tests/kis_liquify_transform_worker_test.cpp
//   libs/image/tests/kis_liquify_transform_worker_test.h:10:10: fatal error:
//   simpletest.h: 没有那个文件或目录
// 往下还要连续解析 KisLiquifyTransformWorker.h/KoColor.h/KoProgressUpdater.h/
// quazip.h/quazipfile.h/testutil.h/kis_algebra_2d.h 等 libs/image + libs/pigment
// + 第三方 quazip 的完整生产依赖链——同一类墙，跟 R-25 Task 1 报告记录的候选
// A/B（kis_kra_loader_test.cpp / psd_layer_section.cpp）撞的是同一堵墙，
// 这些库整体不在本任务 `pk/xml` 的 `locks` 范围内，不是时间盒内能解决的规模。
//
// 退化到计划里已有先例的处理方式（README §11.3 / R-25 Task 1 报告候选 C 的
// graft_run_c 处理方式）：不编译真实文件，改为在这里**逐字符**复刻
// `getWorkerFromIODeviceXml()` 用到 setContent(QIODevice*) 的那几行调用形状
// （类型、参数顺序、参数类型全部对齐），只用 pk/xml/pk/port 自己已交付的
// compat 垫片编译——只证明"签名字面兼容，编译器不报 no matching function"，
// 不证明整份 kis_liquify_transform_worker_test.cpp 能编译。
//
// 原文（未改一个字，逐字抄自
// libs/image/tests/kis_liquify_transform_worker_test.cpp:22-32）：
//
//   // copied from KisLiquifyTransformWorkerBenchmark
//   KisLiquifyTransformWorker* getWorkerFromIODeviceXml(QIODevice* device)
//   {
//       QDomDocument doc;
//
//       doc.setContent(device);
//
//       QDomElement rootElement = doc.documentElement();
//       QDomElement data = rootElement.firstChildElement("data");
//       return KisLiquifyTransformWorker::fromXML(data);
//   }
//
// 下面的 `getWorkerFromIODeviceXmlShape()` 是这段代码的形状对齐版本——去掉了
// `KisLiquifyTransformWorker::fromXML(data)`（libs/image 的生产类，不是本
// 任务要验证的对象），其余每一行 `QDomDocument`/`QDomElement`/`.setContent(`/
// `.documentElement()`/`.firstChildElement(` 的类型与调用形状逐字保留——
// `doc.setContent(device)` 字面上就是真实调用点的写法：只传设备指针，其余
// errorMsg/errorLine/errorColumn 三个参数全部走默认值。

#include <QDomDocument>
#include <QDomElement>
#include <QIODevice>
#include <QString>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

// 最小的内存 QIODevice（= PkStream）子类，只为把一段内存里的 XML 字节喂给
// setContent(QIODevice*)——同 test_document.cpp 里的 MemoryStream，逐字照抄。
class MemoryDevice : public QIODevice
{
public:
    explicit MemoryDevice(std::string data) : m_data(std::move(data)) {}

    pk_int64 size() const override { return static_cast<pk_int64>(m_data.size()); }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 avail = static_cast<pk_int64>(m_data.size()) - pos();
        if (avail <= 0) {
            return 0; // EOF，不是错误。
        }
        const pk_int64 n = maxSize < avail ? maxSize : avail;
        std::memcpy(data, m_data.data() + pos(), static_cast<std::size_t>(n));
        return n;
    }

    pk_int64 writeData(const char *, pk_int64) override { return -1; } // 只读试探用途

private:
    std::string m_data;
};

// 与真实调用点 getWorkerFromIODeviceXml() 逐字对齐的调用形状（trim 掉
// KisLiquifyTransformWorker::fromXML，见文件头注释），返回 data 元素本身
// 供驱动程序断言，代替真实函数的 KisLiquifyTransformWorker* 返回值。
QDomElement getWorkerFromIODeviceXmlShape(QIODevice *device)
{
    QDomDocument doc;

    // 与真实调用点 kis_liquify_transform_worker_test.cpp:27 字面相同的调用：
    //   doc.setContent(device);
    doc.setContent(device);

    QDomElement rootElement = doc.documentElement();
    QDomElement data = rootElement.firstChildElement("data");
    return data;
}

} // namespace

int main()
{
    // 模拟真实调用点里 QuaZipFile/QFile 打开后传给 getWorkerFromIODeviceXml
    // 的设备——内容形状对齐 KisLiquifyTransformWorker::fromXML() 期望的
    // "<liquify><data ... /></liquify>" 顶层结构（真实 .liquify 文件的顶层
    // 元素名不是本试探要验证的对象，这里用最小合法形状即可）。
    MemoryDevice device(std::string("<liquify><data pointsPerLine=\"5\"/></liquify>"));
    device.open(QIODevice::ReadOnly);

    QDomElement data = getWorkerFromIODeviceXmlShape(&device);

    const bool ok = !data.isNull() && data.tagName() == QString("data")
        && data.attribute(QString("pointsPerLine")) == QString("5");
    std::printf(ok ? "shape-call OK: tagName=%s pointsPerLine=%s\n"
                   : "shape-call MISMATCH: tagName=%s pointsPerLine=%s\n",
                data.tagName().PkToUtf8().c_str(),
                data.attribute(QString("pointsPerLine")).PkToUtf8().c_str());
    return ok ? 0 : 1;
}
