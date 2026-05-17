/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Sprint L §5.1 — HTTP client suite (8 functions).
 *
 * Synchronous wrappers over QNetworkAccessManager (blocking via QEventLoop)
 * so WinOLS-style sequential `HttpStart → AddX → Execute → ResponseX` works.
 *
 * Per spec §5.0.7 + §9 risk #7: we must NOT block the UI thread without
 * draining events. HttpExecute spins a local QEventLoop, which is fine.
 */

#include "LuaEngine.h"

#include "sol/sol.hpp"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

namespace lua {

namespace {

// Per-engine HTTP session state — manager + pending request builder.
struct HttpState {
    QNetworkAccessManager mgr;
    QString               url;
    QString               verb;
    QList<QPair<QString, QString>> headers;
    QList<QPair<QString, QString>> params;
    QList<QPair<QString, QString>> files;   // name → filesystem path
    int                   lastStatus = 0;
    QString               lastError;
    QByteArray            lastBody;
};

HttpState &httpState()
{
    static HttpState s;
    return s;
}

QString toQ(const std::string &s) { return QString::fromUtf8(s.c_str(), int(s.size())); }
std::string toS(const QString &q) { auto a = q.toUtf8(); return std::string(a.constData(), size_t(a.size())); }

} // namespace

void bindHttpApi(sol::state &L, LuaEngine *engine)
{
    auto &h = httpState();
    (void)engine;

    // 2.2.8  HttpStart(url, verb="GET") → bool
    L.set_function("HttpStart", [&h](const std::string &url, sol::optional<std::string> verb) -> bool {
        h.url = toQ(url);
        h.verb = verb ? toQ(*verb) : QStringLiteral("GET");
        h.headers.clear();
        h.params.clear();
        h.files.clear();
        h.lastStatus = 0;
        h.lastError.clear();
        h.lastBody.clear();
        return true;
    });

    // 2.2.9  HttpAddHeader(name, value) → bool
    L.set_function("HttpAddHeader", [&h](const std::string &name, const std::string &value) -> bool {
        h.headers.append({ toQ(name), toQ(value) });
        return true;
    });

    // 2.2.10  HttpAddParam(name, value) → bool
    L.set_function("HttpAddParam", [&h](const std::string &name, const std::string &value) -> bool {
        h.params.append({ toQ(name), toQ(value) });
        return true;
    });

    // 2.2.11  HttpAddFile(name, path) → bool
    L.set_function("HttpAddFile", [&h](const std::string &name, const std::string &path) -> bool {
        h.files.append({ toQ(name), toQ(path) });
        return true;
    });

    // 2.2.12  HttpExecute() → number (http status code)
    L.set_function("HttpExecute", [&h]() -> int {
        if (h.url.isEmpty()) { h.lastError = QStringLiteral("HttpStart not called"); return 0; }
        QUrl url(h.url);
        QNetworkRequest req;
        for (const auto &hh : h.headers) req.setRawHeader(hh.first.toUtf8(), hh.second.toUtf8());

        QNetworkReply *reply = nullptr;
        const QString verb = h.verb.toUpper();

        const bool hasMultipart = !h.files.isEmpty();
        const bool hasFormParams = !h.params.isEmpty() && (verb == "POST" || verb == "PUT");

        if (hasMultipart) {
            // multipart/form-data — combine params + files
            QHttpMultiPart *mp = new QHttpMultiPart(QHttpMultiPart::FormDataType);
            for (const auto &p : h.params) {
                QHttpPart part;
                part.setHeader(QNetworkRequest::ContentDispositionHeader,
                    QStringLiteral("form-data; name=\"%1\"").arg(p.first));
                part.setBody(p.second.toUtf8());
                mp->append(part);
            }
            for (const auto &f : h.files) {
                QFile *file = new QFile(f.second);
                if (!file->open(QIODevice::ReadOnly)) {
                    h.lastError = QStringLiteral("cannot open %1").arg(f.second);
                    delete file; delete mp;
                    return 0;
                }
                QHttpPart part;
                part.setHeader(QNetworkRequest::ContentDispositionHeader,
                    QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"")
                        .arg(f.first, QFileInfo(f.second).fileName()));
                part.setBodyDevice(file);
                file->setParent(mp);
                mp->append(part);
            }
            req.setUrl(url);
            reply = (verb == "PUT") ? h.mgr.put(req, mp) : h.mgr.post(req, mp);
            mp->setParent(reply);
        } else if (hasFormParams) {
            // application/x-www-form-urlencoded
            QUrlQuery q;
            for (const auto &p : h.params) q.addQueryItem(p.first, p.second);
            QByteArray body = q.toString(QUrl::FullyEncoded).toUtf8();
            req.setUrl(url);
            req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/x-www-form-urlencoded"));
            reply = (verb == "PUT") ? h.mgr.put(req, body) : h.mgr.post(req, body);
        } else if (verb == "GET" || verb.isEmpty()) {
            // GET — append params as query string
            if (!h.params.isEmpty()) {
                QUrlQuery q;
                for (const auto &p : h.params) q.addQueryItem(p.first, p.second);
                url.setQuery(q);
            }
            req.setUrl(url);
            reply = h.mgr.get(req);
        } else if (verb == "DELETE") {
            req.setUrl(url);
            reply = h.mgr.deleteResource(req);
        } else if (verb == "HEAD") {
            req.setUrl(url);
            reply = h.mgr.head(req);
        } else {
            req.setUrl(url);
            reply = h.mgr.sendCustomRequest(req, verb.toUtf8());
        }

        // Block until finished, processing events so UI stays responsive.
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        h.lastStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        h.lastBody = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            h.lastError = reply->errorString();
        } else {
            h.lastError = QStringLiteral("OK");
        }
        reply->deleteLater();
        return h.lastStatus;
    });

    // 2.2.13  HttpResponseError() → string
    L.set_function("HttpResponseError", [&h]() -> std::string {
        return toS(h.lastError);
    });

    // 2.2.14  HttpResponseString() → string
    L.set_function("HttpResponseString", [&h]() -> std::string {
        return std::string(h.lastBody.constData(), size_t(h.lastBody.size()));
    });

    // 2.2.15  HttpResponseFile(path) → bool
    L.set_function("HttpResponseFile", [&h](const std::string &path) -> bool {
        QFile f(toQ(path));
        if (!f.open(QIODevice::WriteOnly)) return false;
        f.write(h.lastBody);
        f.close();
        return true;
    });
}

} // namespace lua
