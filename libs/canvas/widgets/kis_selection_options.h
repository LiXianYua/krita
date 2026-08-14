/*
 *  SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SELECTION_OPTIONS_H__
#define __KIS_SELECTION_OPTIONS_H__

#include <QList>
#include <QObject>

#include <kritacanvas_export.h>
#include <KisSelectionTags.h>

/**
 * Domain state shared by selection tools.
 *
 * The desktop options panel used to own these values.  The retained canvas
 * graph must keep the tool behavior observable without retaining the desktop
 * adapter, so this class contains only values and change notifications.
 */
class KRITACANVAS_EXPORT KisSelectionOptions : public QObject
{
    Q_OBJECT

public:
    enum ReferenceLayers { CurrentLayer, AllLayers, ColorLabeledLayers };
    Q_ENUM(ReferenceLayers)

    explicit KisSelectionOptions(QObject *parent = nullptr);

    SelectionMode mode() const;
    SelectionAction action() const;
    bool antiAliasSelection() const;
    int growSelection() const;
    bool stopGrowingAtDarkestPixel() const;
    int featherSelection() const;
    ReferenceLayers referenceLayers() const;
    QList<int> selectedColorLabels() const;

    void setMode(SelectionMode value);
    void setAction(SelectionAction value);
    void setAntiAliasSelection(bool value);
    void setGrowSelection(int value);
    void setStopGrowingAtDarkestPixel(bool value);
    void setFeatherSelection(int value);
    void setReferenceLayers(ReferenceLayers value);
    void setSelectedColorLabels(const QList<int> &value);

Q_SIGNALS:
    void modeChanged(SelectionMode mode);
    void actionChanged(SelectionAction action);
    void antiAliasSelectionChanged(bool antiAliasSelection);
    void growSelectionChanged(int growSelection);
    void stopGrowingAtDarkestPixelChanged(bool stopGrowingAtDarkestPixel);
    void featherSelectionChanged(int featherSelection);
    void referenceLayersChanged(ReferenceLayers referenceLayers);
    void selectedColorLabelsChanged();

private:
    SelectionMode m_mode {SHAPE_PROTECTION};
    SelectionAction m_action {SELECTION_REPLACE};
    bool m_antiAliasSelection {true};
    int m_growSelection {0};
    bool m_stopGrowingAtDarkestPixel {false};
    int m_featherSelection {0};
    ReferenceLayers m_referenceLayers {CurrentLayer};
    QList<int> m_selectedColorLabels;
};

#endif
