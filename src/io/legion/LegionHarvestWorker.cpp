/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * LegionHarvestWorker implementation.
 */

#include "io/legion/LegionHarvestWorker.h"
#include "io/legion/Legion.h"

#include "io/winols/SimilarityIndex.h"
#include "io/winols/RomFingerprint.h"
#include "io/ols/OlsImporter.h"

#include <QFile>
#include <QFileInfo>
#include <QMetaType>

namespace legion {

namespace {
// Auto-register QVector<LegionVoice> the first time the worker compiles in.
const int kVoiceVectorMetaId =
    qRegisterMetaType<QVector<legion::LegionVoice>>("QVector<legion::LegionVoice>");
}

LegionHarvestWorker::LegionHarvestWorker(QByteArray baseline,
                                         QString    selfPath,
                                         int        minPercent,
                                         int        maxVoices,
                                         QObject   *parent)
    : QObject(parent),
      m_baseline(std::move(baseline)),
      m_selfPath(std::move(selfPath)),
      m_minPercent(minPercent),
      m_maxVoices(maxVoices)
{}

void LegionHarvestWorker::run()
{
    QVector<LegionVoice> voices;
    voices.reserve(m_maxVoices);

    if (m_baseline.isEmpty()) {
        emit finished(voices, m_cancel.load());
        return;
    }

    // ── Open the catalog index on the worker thread (each thread gets
    //    its own SQLite connection name; SimilarityIndex handles that).
    winols::SimilarityIndex idx;
    QString err;
    if (!idx.open(&err)) {
        emit finished(voices, m_cancel.load());
        return;
    }

    QVector<winols::SimilarityMatch> matches;
    try {
        winols::RomFingerprint needle = winols::fingerprint(
            QByteArrayView(m_baseline));
        matches = idx.findSimilar(needle, m_minPercent, m_maxVoices * 4);
    } catch (...) {
        emit finished(voices, m_cancel.load());
        return;
    }

    const int total = matches.size();
    int processed = 0;

    for (const auto &m : matches) {
        if (m_cancel.load()) break;
        if (voices.size() >= m_maxVoices) break;

        ++processed;
        emit progress(processed, total, m.path);

        if (!m_selfPath.isEmpty() &&
            QFileInfo(m.path) == QFileInfo(m_selfPath)) continue;

        QFile f(m.path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray bytes = f.readAll();
        f.close();
        if (bytes.isEmpty()) continue;

        ols::OlsImportResult res;
        try { res = ols::OlsImporter::importFromBytes(bytes); }
        catch (...) { continue; }
        if (!res.error.isEmpty() || res.versions.size() < 2) continue;

        const QByteArray &v0 = res.versions.first().romData;
        const QByteArray &v1 = res.versions.last().romData;
        if (v0.size() != v1.size() || v0.isEmpty()) continue;

        LegionVoice voice;
        voice.sourcePath = m.path;
        voice.similarity = int(m.score.wholePct());
        voice.regions    = detectRegions(v0, v1, 16);
        for (const auto &r : voice.regions) {
            for (uint32_t a = r.startAddr; a <= r.endAddr; ++a) {
                if (a < uint32_t(v0.size()) && a < uint32_t(v1.size())
                    && v0[int(a)] != v1[int(a)]) {
                    voice.addressSet.insert(a);
                }
            }
        }
        if (!voice.addressSet.isEmpty()) voices.append(std::move(voice));
    }

    emit finished(voices, m_cancel.load());
}

}   // namespace legion
