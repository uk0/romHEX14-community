/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Map auto-detect corpus harness (issue #25)
 * ===========================================
 *
 * Runs ols::MapAutoDetect over every labelled ROM in a corpus directory and
 * prints per-file and aggregate precision/recall, so changes to
 * MapAutoDetect.cpp or its scoring priors can be compared run-over-run.
 *
 * Corpus layout (default dir: <repo>/testdata/mapdetect, or pass a dir as
 * argv[1]):
 *
 *   <name>.bin             the raw ROM to scan
 *   <name>.expected.json   ground truth:
 *                            { "baseAddress": 0,
 *                              "maps": [ {"address": "0x1A30",
 *                                          "width": 16, "height": 12}, … ] }
 *   <name>.kp              alternative ground truth: a KP map pack whose
 *                          entries are used when no .expected.json exists
 *
 * A detected candidate counts as a hit when its ROM-relative address equals
 * an expected map address. The harness never fails on poor precision — it is
 * a tracking tool, not a gate. Exit codes: 0 = ran (or empty corpus),
 * 2 = malformed corpus file.
 */

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include "../src/io/ols/MapAutoDetect.h"
#include "../src/io/ols/KpImporter.h"

struct ExpectedMap {
    quint32 address = 0;
    int     width   = 0;   // 0 = unknown
    int     height  = 0;
};

static bool parseAddress(const QJsonValue &v, quint32 *out)
{
    if (v.isDouble()) { *out = (quint32)v.toDouble(); return true; }
    QString s = v.toString().trimmed();
    int base = 10;
    if (s.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) { s.remove(0, 2); base = 16; }
    else if (s.startsWith(QLatin1Char('$')))                    { s.remove(0, 1); base = 16; }
    bool ok = false;
    *out = s.toUInt(&ok, base);
    return ok;
}

static bool loadExpectedJson(const QString &path, QVector<ExpectedMap> *out,
                             quint32 *baseAddress)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        qCritical() << "Malformed JSON:" << path << pe.errorString();
        exit(2);
    }
    const QJsonObject root = doc.object();
    *baseAddress = (quint32)root["baseAddress"].toDouble(0);
    for (const QJsonValue &v : root["maps"].toArray()) {
        const QJsonObject mo = v.toObject();
        ExpectedMap e;
        if (!parseAddress(mo["address"], &e.address)) {
            qCritical() << "Bad address in" << path << ":" << mo["address"];
            exit(2);
        }
        e.width  = mo["width"].toInt(0);
        e.height = mo["height"].toInt(0);
        out->append(e);
    }
    return true;
}

static bool loadExpectedKp(const QString &path, QVector<ExpectedMap> *out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const ols::KpImportResult res = ols::KpImporter::importFromBytes(f.readAll());
    if (!res.error.isEmpty()) {
        qCritical() << "KP parse failed:" << path << res.error;
        exit(2);
    }
    for (const MapInfo &mi : res.maps) {
        ExpectedMap e;
        e.address = mi.address;
        e.width   = mi.dimensions.x;
        e.height  = mi.dimensions.y;
        out->append(e);
    }
    return true;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QString dir = QStringLiteral(TESTDATA_DIR) + QStringLiteral("/mapdetect");
    if (argc > 1) dir = QString::fromLocal8Bit(argv[1]);

    const QStringList roms = QDir(dir).entryList(
        {QStringLiteral("*.bin")}, QDir::Files, QDir::Name);
    if (roms.isEmpty()) {
        qInfo() << "No corpus ROMs found in" << dir;
        qInfo() << "Drop <name>.bin + <name>.expected.json (or <name>.kp) pairs"
                   " there to track auto-detect precision/recall.";
        return 0;
    }

    int totalDetected = 0, totalExpected = 0, totalMatched = 0, totalDimOk = 0;

    for (const QString &romName : roms) {
        const QString base = dir + QLatin1Char('/')
            + romName.left(romName.size() - 4);   // strip ".bin"

        QVector<ExpectedMap> expected;
        quint32 baseAddress = 0;
        if (!loadExpectedJson(base + QStringLiteral(".expected.json"),
                              &expected, &baseAddress)
            && !loadExpectedKp(base + QStringLiteral(".kp"), &expected)) {
            qInfo() << "SKIP (no .expected.json / .kp):" << romName;
            continue;
        }
        if (expected.isEmpty()) {
            qInfo() << "SKIP (truth file lists no maps):" << romName;
            continue;
        }

        QFile rf(dir + QLatin1Char('/') + romName);
        if (!rf.open(QIODevice::ReadOnly)) {
            qCritical() << "Cannot read" << romName;
            exit(2);
        }
        const QByteArray rom = rf.readAll();

        ols::MapAutoDetectOptions opts;   // defaults = what the app ships
        const QVector<ols::MapCandidate> found =
            ols::MapAutoDetect::scan(rom, /*baseAddress=*/0, opts);

        QSet<quint32> expectedAddrs;
        QHash<quint32, const ExpectedMap *> byAddr;
        for (const ExpectedMap &e : expected) {
            // Truth addresses may be absolute; normalise to ROM offsets.
            const quint32 off = (e.address >= baseAddress)
                                ? e.address - baseAddress : e.address;
            expectedAddrs.insert(off);
            byAddr.insert(off, &e);
        }

        int matched = 0, dimOk = 0;
        for (const auto &c : found) {
            if (!expectedAddrs.contains(c.romAddress)) continue;
            ++matched;
            const ExpectedMap *e = byAddr.value(c.romAddress);
            if (e && (e->width == 0
                      || (int(c.width) == e->width && int(c.height) == e->height)))
                ++dimOk;
            else if (e)
                qInfo().noquote() << QStringLiteral(
                    "    dim mismatch @0x%1: detected %2×%3 (%4B), expected %5×%6")
                    .arg(c.romAddress, 0, 16).arg(c.width).arg(c.height)
                    .arg(c.cellBytes).arg(e->width).arg(e->height);
        }

        // Nothing matched at all: dump the top candidates so corpus address
        // mismatches (absolute vs ROM-relative, layout shifts) are debuggable.
        if (matched == 0 && !found.isEmpty()) {
            QVector<ols::MapCandidate> top = found;
            std::sort(top.begin(), top.end(),
                      [](const ols::MapCandidate &a, const ols::MapCandidate &b) {
                          return a.score > b.score;
                      });
            for (int i = 0; i < qMin(5, top.size()); ++i)
                qInfo().noquote() << QStringLiteral("    candidate 0x%1 %2×%3 score %4")
                    .arg(top[i].romAddress, 0, 16).arg(top[i].width)
                    .arg(top[i].height).arg(top[i].score, 0, 'f', 1);
        }

        const double prec = found.isEmpty() ? 0.0 : 100.0 * matched / found.size();
        const double rec  = 100.0 * matched / expected.size();
        qInfo().noquote() << QStringLiteral(
            "%1: detected %2, expected %3, matched %4 (dims ok %5) — "
            "precision %6%, recall %7%")
            .arg(romName).arg(found.size()).arg(expected.size())
            .arg(matched).arg(dimOk)
            .arg(prec, 0, 'f', 1).arg(rec, 0, 'f', 1);

        totalDetected += found.size();
        totalExpected += expected.size();
        totalMatched  += matched;
        totalDimOk    += dimOk;
    }

    if (totalExpected > 0) {
        const double prec = totalDetected ? 100.0 * totalMatched / totalDetected : 0.0;
        const double rec  = 100.0 * totalMatched / totalExpected;
        qInfo().noquote() << QStringLiteral(
            "TOTAL: detected %1, expected %2, matched %3 (dims ok %4) — "
            "precision %5%, recall %6%")
            .arg(totalDetected).arg(totalExpected).arg(totalMatched)
            .arg(totalDimOk).arg(prec, 0, 'f', 1).arg(rec, 0, 'f', 1);
    }
    return 0;
}
