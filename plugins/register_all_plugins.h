/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef REGISTER_ALL_PLUGINS_H
#define REGISTER_ALL_PLUGINS_H

// D-12 静态注册聚合器入口。M5 合拢时由 app/pk 消费者链入 kritaplugin_registry 并调用一次。
void registerAllPlugins();

#endif
