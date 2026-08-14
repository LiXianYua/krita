/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_FEEDBACK_H
#define KIS_CANVAS_FEEDBACK_H

#include <Qt>

#include <kritacanvas_export.h>

class QIcon;
class QString;

/**
 * Narrow presentation port for transient feedback produced by canvas tools.
 *
 * Tool algorithms do not need access to the application view or window. A
 * concrete canvas may present the message using its native UI, or omit the
 * capability entirely in a headless host.
 */
class KRITACANVAS_EXPORT KisCanvasFeedback
{
public:
    enum class Priority {
        High,
        Medium,
        Low
    };

    virtual ~KisCanvasFeedback();

    virtual void showFloatingMessage(const QString &message,
                                     const QIcon &icon,
                                     int timeout = 4500,
                                     Priority priority = Priority::Medium,
                                     int alignment = Qt::AlignCenter | Qt::TextWordWrap) = 0;
};

#endif // KIS_CANVAS_FEEDBACK_H
