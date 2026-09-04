#include "PkDom.h"

static void collectByTagName(PkDomNode *n, const std::string &tag, PkDomNodeList &out) {
    for (PkDomNode *c = n->firstChild(); c; c = c->nextSibling()) {
        if (PkDomElement *e = c->toElement()) {
            if (e->tagName() == tag) out.push_back(e);
            collectByTagName(e, tag, out);
        }
    }
}

PkDomNodeList PkDomElement::elementsByTagName(const std::string &tag) const {
    PkDomNodeList out;
    collectByTagName(const_cast<PkDomElement *>(this), tag, out);
    return out;
}
