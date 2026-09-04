#ifndef PK_DOMDOCUMENT_H
#define PK_DOMDOCUMENT_H

// PkDomDocument —— QDomDocument 的零 Qt 替代（R-50 锁内文件）。
#include "PkDom.h"
#include <string>

class PkDomDocument : public PkDomNode {
public:
    PkDomDocument() : PkDomNode(DocumentNode, "#document", this) { m_doc = this; }

    PkDomElement *createElement(const std::string &tag) { return new PkDomElement(this, tag); }
    PkDomText *createTextNode(const std::string &data) { return new PkDomText(this, data); }

    PkDomElement *documentElement() const { return m_root; }
    void setDocumentElement(PkDomElement *e) { m_root = e; }

    // 解析入口：用 pugixml 把 XML 文本解析成 PkDom 树（避免与 pk/xml 锁重叠——
    // 这里只用 pugixml 库，不碰 pk/xml 的源码）。返回 false 时通过 errorMsg/
    // errorLine/errorCol 回传 pugixml 的诊断信息（行号近似为字节偏移）。
    bool setContent(const std::string &data, bool namespaceProcessing = false,
                    std::string *errorMsg = nullptr, int *errorLine = nullptr,
                    int *errorCol = nullptr);

private:
    PkDomElement *m_root = nullptr;
};

#endif // PK_DOMDOCUMENT_H
