/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "communityupdatechecker.h"
#include "version.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>

static const char kReleasesApi[] =
    "https://api.github.com/repos/ctabuyo/romHEX14-community/releases";

CommunityUpdateChecker::CommunityUpdateChecker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void CommunityUpdateChecker::checkForUpdates()
{
    QNetworkRequest req(QUrl(QString::fromUtf8(kReleasesApi)));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(10000);

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Update check failed:" << reply->errorString();
            return;
        }
        // qWarning() << "Update check: HTTP" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj;
        if (doc.isArray()) {
            const auto arr = doc.array();
            if (arr.isEmpty()) return;
            obj = arr.first().toObject();
        } else if (doc.isObject()) {
            obj = doc.object();
        } else {
            return;
        }
        QString tag = obj.value("tag_name").toString();
        const QString url = obj.value("html_url").toString();

        // Tag format: "romhex14-community1.0.0-beta3" — strip prefix
        tag.remove("romhex14-community");

        // Compare versions: strip pre-release suffix for numeric comparison
        auto parseVer = [](const QString &s) -> QPair<QVersionNumber, QString> {
            // "1.0.0-beta3" -> numeric "1.0.0", suffix "beta3"
            int dash = s.indexOf('-');
            if (dash < 0)
                return { QVersionNumber::fromString(s), {} };
            return { QVersionNumber::fromString(s.left(dash)), s.mid(dash + 1) };
        };

        auto [remoteVer, remoteSuffix] = parseVer(tag);
        auto [localVer,  localSuffix]  = parseVer(QString::fromUtf8(RX14_VERSION_STRING));


        if (remoteVer.isNull() || localVer.isNull())
            return;

        // Remote is newer if: higher version number, OR same version with
        // "later" pre-release (beta4 > beta3), OR remote is stable (no suffix)
        // while local is pre-release
        bool newer = false;
        int cmp = QVersionNumber::compare(remoteVer, localVer);
        if (cmp > 0) {
            newer = true;
        } else if (cmp == 0) {
            if (localSuffix.isEmpty() && remoteSuffix.isEmpty())
                newer = false; // same stable version
            else if (!localSuffix.isEmpty() && remoteSuffix.isEmpty())
                newer = true;  // remote is stable, local is pre-release
            else if (!localSuffix.isEmpty() && !remoteSuffix.isEmpty())
                newer = (remoteSuffix > localSuffix); // lexicographic: beta4 > beta3
        }

        // qWarning() << "Update check: newer=" << newer << "remoteSuffix=" << remoteSuffix << "localSuffix=" << localSuffix;
        if (newer)
            emit updateAvailable(tag, url);
    });
}
