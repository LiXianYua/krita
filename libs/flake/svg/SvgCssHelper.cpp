/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkXmlCompat.h>

#include "SvgCssHelper.h"
#include <FlakeDebug.h>
#include <pk/container/PkContainerAlgo.h>
#include <utility>
#include <regex>
#include <string>
#include <vector>

// PkString 缺单字符成员方法与 simplified()/indexOf()/split(SkipEmptyParts)，
// 这里补一组文件局部小工具，语义对齐原字符串类型的对应物。
namespace {

bool pkIsSpace(char16_t c)
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r'
        || c == u'\v' || c == u'\f' || c == 0x00A0 || c == 0x2028 || c == 0x2029;
}

// 单个 UTF-16 码元 → PkString（仅 BMP；CSS 选择器词法里的单字符都是 ASCII）。
PkString pkCharString(char16_t c)
{
    std::string s;
    if (c < 0x80) {
        s.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
        s.push_back(static_cast<char>(0xC0 | (c >> 6)));
        s.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
        s.push_back(static_cast<char>(0xE0 | (c >> 12)));
        s.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    return PkString(s.c_str());
}

int pkIndexOf(const PkString &s, char16_t c)
{
    for (int i = 0; i < s.size(); ++i) {
        if (s.at(i) == c)
            return i;
    }
    return -1;
}

// 两端去空白、内部连续空白折叠成单个空格（原 simplified() 语义）。
PkString pkSimplified(const PkString &s)
{
    PkString result;
    bool pendingSpace = false;
    bool hasWritten = false;
    for (int i = 0; i < s.size(); ++i) {
        const char16_t c = s.at(i);
        if (pkIsSpace(c)) {
            if (hasWritten)
                pendingSpace = true;
        } else {
            if (pendingSpace) {
                result.append(PkString(" "));
                pendingSpace = false;
            }
            result.append(pkCharString(c));
            hasWritten = true;
        }
    }
    return result;
}

// 按分隔符切分并跳过空段（原 split(sep, SkipEmptyParts) 语义）：
// PkString::split 保留空段，这里滤掉。
PkStringList pkSplitSkipEmpty(const PkString &s, char16_t sep)
{
    PkStringList result;
    const std::vector<PkString> parts = s.split(sep);
    for (const PkString &p : parts) {
        if (!p.isEmpty())
            result.append(p);
    }
    return result;
}

}

/// Token types used for tokenizing complex selectors
enum CssTokenType {
    SelectorToken,  ///< a selector token
    CombinatorToken ///< a combinator token
};

/// A token used for tokenizing complex selectors
typedef std::pair<CssTokenType, PkString> CssToken;

/// Selector base class, merely an interface
class CssSelectorBase
{
public:
    virtual ~CssSelectorBase() {}
    /// Matches the given element
    virtual bool match(const PkXmlElement &) = 0;
    /// Returns string representation of selector
    virtual PkString toString() const { return PkString(); }
    /**
     * Returns priority of selector
     * see http://www.w3.org/TR/1998/REC-CSS2-19980512/cascade.html#specificity
     */
    virtual int priority() { return 0; }
};

/// Universal selector, matching anything
class UniversalSelector : public CssSelectorBase
{
public:
    bool match(const PkXmlElement &) override
    {
        // matches always
        return true;
    }
    PkString toString() const override
    {
        return PkString("*");
    }
};

/// Type selector, matching the type of an element
class TypeSelector : public CssSelectorBase
{
public:
    TypeSelector(const PkString &type)
    : m_type(type)
    {
    }
    bool match(const PkXmlElement &e) override
    {
        return e.tagName() == m_type;
    }
    PkString toString() const override
    {
        return m_type;
    }
    int priority() override
    {
        return 1;
    }

private:
    PkString m_type;
};

/// Id selector, matching the id attribute
class IdSelector : public CssSelectorBase
{
public:
    IdSelector(const PkString &id)
    : m_id(id)
    {
        if (id.startsWith(PkString("#")))
            m_id = id.mid(1);
    }
    bool match(const PkXmlElement &e) override
    {
        return e.attribute("id") == m_id;
    }
    PkString toString() const override
    {
        return PkString("#") + m_id;
    }
    int priority() override
    {
        return 100;
    }
private:
    PkString m_id;
};

/// Attribute selector, matching existence or content of attributes
class AttributeSelector : public CssSelectorBase
{
public:
    AttributeSelector(const PkString &attribute)
    : m_type(Unknown)
    {
        PkString pattern = attribute;
        if (pattern.startsWith(PkString("[")))
            pattern = pattern.mid(1);
        if (pattern.size() > 0 && pattern.at(pattern.size()-1) == u']')
            pattern = pattern.left(pattern.size()-1);
        int equalPos = pkIndexOf(pattern, u'=');
        if (equalPos == -1) {
            m_type = Exists;
            m_attribute = pattern;
        } else if (equalPos > 0){
            if (pattern[equalPos-1] == u'~') {
                m_attribute = pattern.left(equalPos-1);
                m_type = InList;
            } else if(pattern[equalPos-1] == u'|') {
                m_attribute = pattern.left(equalPos-1) + PkString("-");
                m_type = StartsWith;
            } else {
                m_attribute = pattern.left(equalPos);
                m_type = Equals;
            }
            m_value = pattern.mid(equalPos+1);
            if (m_value.startsWith(PkString("\"")))
                m_value = m_value.mid(1);
            if (m_value.size() > 0 && m_value.at(m_value.size()-1) == u'"')
                m_value = m_value.left(m_value.size()-1);
        }
    }

    bool match(const PkXmlElement &e) override
    {
        switch(m_type) {
            case Exists:
                return e.hasAttribute(m_attribute);
                break;
            case Equals:
                return e.attribute(m_attribute) == m_value;
                break;
            case InList:
                {
                    PkStringList tokens = pkSplitSkipEmpty(e.attribute(m_attribute), u' ');
                    return tokens.contains(m_value);
                }
                break;
            case StartsWith:
                return e.attribute(m_attribute).startsWith(m_value);
                break;
            default:
                return false;
        }
    }
    PkString toString() const override
    {
        PkString str = PkString("[");
        str += m_attribute;
        if (m_type == Equals) {
            str += PkString("=");
        } else if (m_type == InList) {
            str += PkString("~=");
        } else if (m_type == StartsWith) {
            str += PkString("|=");
        }
        str += m_value;
        str += PkString("]");
        return str;
    }
    int priority() override
    {
        return 10;
    }

private:
    enum MatchType {
        Unknown,   ///< unknown    -> error state
        Exists,    ///< [att]      -> attribute exists
        Equals,    ///< [att=val]  -> attribute value matches exactly val
        InList,    ///< [att~=val] -> attribute is whitespace separated list where one is val
        StartsWith ///< [att|=val] -> attribute starts with val-
    };
    PkString m_attribute;
    PkString m_value;
    MatchType m_type;
};

/// Pseudo-class selector
class PseudoClassSelector : public CssSelectorBase
{
public:
    PseudoClassSelector(const PkString &pseudoClass)
    : m_pseudoClass(pseudoClass)
    {
    }

    bool match(const PkXmlElement &e) override
    {
        if (m_pseudoClass == ":first-child") {
            PkXmlNode parent = e.parentNode();
            if (parent.isNull()) {
                return false;
            }
            // 原实现找 parent 的第一个元素子节点再与 e 比相等；PkXmlNode 无
            // operator==，改为等价判定：e 前面没有元素兄弟 ⇔ e 是第一个元素子节点。
            return e.previousSiblingElement().isNull();
        } else {
            return false;
        }
    }
    PkString toString() const override
    {
        return m_pseudoClass;
    }
    int priority() override
    {
        return 10;
    }

private:
    PkString m_pseudoClass;
};

/// A simple selector, i.e. a type/universal selector followed by attribute, id or pseudo-class selectors
class CssSimpleSelector : public CssSelectorBase
{
public:
    CssSimpleSelector(const PkString &token)
    : m_token(token)
    {
        compile();
    }
    ~CssSimpleSelector() override
    {
        qDeleteAll(m_selectors);
    }

    bool match(const PkXmlElement &e) override
    {
        Q_FOREACH (CssSelectorBase *s, m_selectors) {
            if (!s->match(e))
                return false;
        }

        return true;
    }

    PkString toString() const override
    {
        PkString str;
        Q_FOREACH (CssSelectorBase *s, m_selectors) {
            str += s->toString();
        }
        return str;
    }
    int priority() override
    {
        int p = 0;
        Q_FOREACH (CssSelectorBase *s, m_selectors) {
            p += s->priority();
        }
        return p;
    }

private:
    void compile()
    {
        if (m_token == PkString("*")) {
            m_selectors.append(new UniversalSelector());
            return;
        }

        enum {
            Start,
            Finish,
            Bad,
            InType,
            InId,
            InAttribute,
            InClassAttribute,
            InPseudoClass
        } state;

        // add terminator to string
        PkString expr = m_token + PkString::PkFromUtf8("\0", 1);
        int i = 0;
        state = Start;

        PkString token;
        PkString sep("#[:.");
        // split into base selectors
        while((state != Finish) && (state != Bad) && (i < expr.size())) {
            char16_t ch = expr[i];
            switch(state) {
                case Start:
                    token += pkCharString(ch);
                    if (ch == u'#')
                        state = InId;
                    else if (ch == u'[')
                        state = InAttribute;
                    else if (ch == u':')
                        state = InPseudoClass;
                    else if (ch == u'.')
                        state = InClassAttribute;
                    else if (ch != u'*')
                        state = InType;
                    break;
                case InAttribute:
                    if (ch == u'\0') {
                        // reset state and token string
                        state = Finish;
                        token = PkString();
                        continue;
                    } else {
                        token += pkCharString(ch);
                        if (ch == u']') {
                            m_selectors.append(new AttributeSelector(token));
                            state = Start;
                            token = PkString();
                        }
                    }
                    break;
                case InType:
                case InId:
                case InClassAttribute:
                case InPseudoClass:
                    // are we at the start of the next selector or even finished?
                    if (sep.contains(pkCharString(ch)) || ch == u'\0') {
                        if (state == InType)
                            m_selectors.append(new TypeSelector(token));
                        else if (state == InId)
                            m_selectors.append(new IdSelector(token));
                        else if ( state == InClassAttribute)
                            m_selectors.append(new AttributeSelector(PkString("[class~=") + token.mid(1) + PkString("]")));
                        else if (state == InPseudoClass) {
                            m_selectors.append(new PseudoClassSelector(token));
                        }
                        // reset state and token string
                        state = ch == u'\0' ? Finish : Start;
                        token = PkString();
                        continue;
                    } else {
                        // append character to current token
                        if (ch != u'\0')
                            token += pkCharString(ch);
                    }
                    break;
                default:
                    break;
            }
            i++;
        }
    }

    PkList<CssSelectorBase*> m_selectors;
    PkString m_token;
};

/// Complex selector, i.e. a combination of simple selectors
class CssComplexSelector : public CssSelectorBase
{
public:
    CssComplexSelector(const PkList<CssToken> &tokens)
    {
        compile(tokens);
    }
    ~CssComplexSelector() override
    {
        qDeleteAll(m_selectors);
    }
    PkString toString() const override
    {
        PkString str;
        int selectorCount = m_selectors.count();
        if (selectorCount) {
            for(int i = 0; i < selectorCount-1; ++i) {
                str += m_selectors[i]->toString() +
                       pkCharString(m_combinators[i]);
            }
            str += m_selectors.last()->toString();
        }
        return str;
    }

    bool match(const PkXmlElement &e) override
    {
        int selectorCount = m_selectors.count();
        int combinatorCount = m_combinators.size();
        // check count of selectors and combinators
        if (selectorCount-combinatorCount != 1)
            return false;

        PkXmlElement currentElement = e;

        // match in reverse order
        for(int i = 0; i < selectorCount; ++i) {
            CssSelectorBase * curr = m_selectors[selectorCount-1-i];
            if (!curr->match(currentElement)) {
                return false;
            }
            // last selector and still there -> rule matched completely
            if(i == selectorCount-1)
                return true;

            CssSelectorBase * next = m_selectors[selectorCount-1-i-1];
            char16_t combinator = m_combinators[combinatorCount-1-i];
            if (combinator == u' ') {
                bool matched = false;
                // descendant combinator
                PkXmlNode parent = currentElement.parentNode();
                while(!parent.isNull()) {
                    currentElement = parent.toElement();
                    if (next->match(currentElement)) {
                        matched = true;
                        break;
                    }
                    parent = currentElement.parentNode();
                }
                if(!matched)
                    return false;
            } else if (combinator == u'>') {
                // child selector
                PkXmlNode parent = currentElement.parentNode();
                if (parent.isNull())
                    return false;
                PkXmlElement parentElement = parent.toElement();
                if (next->match(parentElement)) {
                    currentElement = parentElement;
                } else {
                    return false;
                }
            } else if (combinator == u'+') {
                PkXmlNode neighbor = currentElement.previousSibling();
                while(!neighbor.isNull() && !neighbor.isElement())
                    neighbor = neighbor.previousSibling();
                if (neighbor.isNull() || !neighbor.isElement())
                    return false;
                PkXmlElement neighborElement = neighbor.toElement();
                if (next->match(neighborElement)) {
                    currentElement = neighborElement;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
        return true;
    }
    int priority() override
    {
        int p = 0;
        Q_FOREACH (CssSelectorBase *s, m_selectors) {
            p += s->priority();
        }
        return p;
    }

private:
    void compile(const PkList<CssToken> &tokens)
    {
        Q_FOREACH (const CssToken &token, tokens) {
            if(token.first == SelectorToken) {
                m_selectors.append(new CssSimpleSelector(token.second));
            } else {
                m_combinators += token.second;
            }
        }
    }

    PkString m_combinators;
    PkList<CssSelectorBase*> m_selectors;
};

/// A group of selectors (comma separated in css style sheet)
typedef PkList<CssSelectorBase*> SelectorGroup;
/// A css rule consisting of group of selectors corresponding to a style
typedef std::pair<SelectorGroup, PkString> CssRule;

class SvgCssHelper::Private
{
public:
    ~Private()
    {
        Q_FOREACH (const CssRule &rule, cssRules) {
            qDeleteAll(rule.first);
        }
    }

    SelectorGroup parsePattern(const PkString &pattern)
    {
        SelectorGroup group;

        PkStringList selectors = pkSplitSkipEmpty(pattern, u',');
        for (int i = 0; i < selectors.count(); ++i ) {
            CssSelectorBase * selector = compileSelector(pkSimplified(selectors[i]));
            if (selector)
                group.append(selector);
        }
        return group;
    }

    PkList<CssToken> tokenize(const PkString &selector)
    {
        // add terminator to string
        PkString expr = selector + PkString::PkFromUtf8("\0", 1);
        enum {
            Finish,
            Bad,
            InCombinator,
            InSelector
        } state;

        char16_t combinator = u'\0';
        int selectorStart = 0;

        PkList<CssToken> tokenList;

        char16_t ch = expr[0];
        if (pkIsSpace(ch) || ch == u'>' || ch == u'+') {
            debugFlake << "selector starting with combinator is not allowed:" << selector;
            return tokenList;
        } else {
            state = InSelector;
            selectorStart = 0;
        }
        int i = 1;

        // split into simple selectors and combinators
        while((state != Finish) && (state != Bad) && (i < expr.size())) {
            char16_t ch = expr[i];
            switch(state) {
                case InCombinator:
                    // consume as long as there a combinator characters
                    if( ch == u'>' || ch == u'+') {
                        if( ! pkIsSpace(combinator) ) {
                            // two non whitespace combinators in sequence are not allowed
                            state = Bad;
                        } else {
                            // switch combinator
                            combinator = ch;
                        }
                    } else if (!pkIsSpace(ch)) {
                        tokenList.append(CssToken(CombinatorToken, pkCharString(combinator)));
                        state = InSelector;
                        selectorStart = i;
                        combinator = u'\0';
                    }
                    break;
                case InSelector:
                    // consume as long as there a non combinator characters
                    if (pkIsSpace(ch) || ch == u'>' || ch == u'+') {
                        state = InCombinator;
                        combinator = ch;
                    } else if (ch == u'\0') {
                        state = Finish;
                    }
                    if (state != InSelector) {
                        PkString simpleSelector = selector.mid(selectorStart, i-selectorStart);
                        tokenList.append(CssToken(SelectorToken, simpleSelector));
                    }
                    break;
                default:
                    break;
            }
            i++;
        }

        return tokenList;
    }

    CssSelectorBase * compileSelector(const PkString &selector)
    {
        PkList<CssToken> tokenList = tokenize(selector);
        if (tokenList.isEmpty())
            return 0;

        if (tokenList.count() == 1) {
            // simple selector
            return new CssSimpleSelector(tokenList.first().second);
        } else if (tokenList.count() > 2) {
            // complex selector
            return new CssComplexSelector(tokenList);
        }
        return 0;
    }

    PkMap<PkString, PkString> cssStyles;
    PkList<CssRule> cssRules;
};

SvgCssHelper::SvgCssHelper()
: d(new Private())
{
}

SvgCssHelper::~SvgCssHelper()
{
    delete d;
}

void SvgCssHelper::parseStylesheet(const PkXmlElement &e)
{
    PkString data;

    if (e.hasChildNodes()) {
        PkXmlNode c = e.firstChild();
        if (c.isCDATASection()) {
            PkXmlCDATASection cdata = c.toCDATASection();
            data = pkSimplified(cdata.data());
        } else if (c.isText()) {
            PkXmlText text = c.toText();
            data = pkSimplified(text.data());
        }
    }
    if (data.isEmpty())
        return;

    // Remove comments
    // NOTE: that must not be greedy as per definition of css-comments,
    //       the first closing '*/' sequence closes the entire comment
    //       block
    std::regex commentExp("\\/\\*.*?\\*\\/");
    data = PkString(std::regex_replace(data.PkToUtf8(), commentExp, "").c_str());

    PkStringList defs = pkSplitSkipEmpty(data, u'}');
    for (int i = 0; i < defs.count(); ++i) {
        std::vector<PkString> def = defs[i].split(u'{');
        if (def.size() != 2)
            continue;
        PkString pattern = pkSimplified(def[0]);
        if (pattern.isEmpty())
            break;
        PkString style = pkSimplified(def[1]);
        if (style.isEmpty())
            break;
        PkStringList selectors = pkSplitSkipEmpty(pattern, u',');
        for (int i = 0; i < selectors.count(); ++i ) {
            PkString selector = pkSimplified(selectors[i]);
            d->cssStyles[selector] = style;
        }
        SelectorGroup group = d->parsePattern(pattern);
        d->cssRules.append(CssRule(group, style));
    }
}

PkStringList SvgCssHelper::matchStyles(const PkXmlElement &element) const
{
    PkMap<int, PkString> prioritizedRules;
    // match rules to element
    Q_FOREACH (const CssRule &rule, d->cssRules) {
        Q_FOREACH (CssSelectorBase *s, rule.first) {
            bool matched = s->match(element);
            if (matched)
                prioritizedRules[s->priority()] = rule.second;
        }
    }

    // css style attribute has the priority of 100
    PkString styleAttribute = pkSimplified(element.attribute("style"));
    if (!styleAttribute.isEmpty())
        prioritizedRules[100] = styleAttribute;

    PkStringList cssStyles;
    // add matching styles in correct order to style list
    PkMapIterator<int, PkString> it(prioritizedRules);
    while (it.hasNext()) {
        it.next();
        cssStyles.append(it.value());
    }

    return cssStyles;
}
