/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLUGIN_D_INPUT_POLICY_TEST_H
#define PLUGIN_D_INPUT_POLICY_TEST_H

#include <simpletest.h>

class PluginDInputPolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void enclosePathPolicyMatchesQt515();
    void transformModifierPolicyMatchesQt515();
};

#endif
