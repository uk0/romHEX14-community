/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

class CommunityUpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit CommunityUpdateChecker(QObject *parent = nullptr);
    void checkForUpdates();

signals:
    void updateAvailable(const QString &latestVersion, const QString &releaseUrl);

private:
    QNetworkAccessManager *m_nam = nullptr;
};
