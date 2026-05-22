/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/WolsTwinFinder.h"
#include "io/winols/WinOlsConfig.h"

#include "debug/DebugLog.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPair>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

#if __has_include(<zlib.h>)
#  include <zlib.h>
#else
#  include <QtZlib/zlib.h>
#endif

namespace winols {

namespace {

inline int hexVal(QChar c)
{
    const ushort u = c.unicode();
    if (u >= '0' && u <= '9') return u - '0';
    if (u >= 'a' && u <= 'f') return u - 'a' + 10;
    if (u >= 'A' && u <= 'F') return u - 'A' + 10;
    return -1;
}

// Fraction of differing bits between two equal-length hex strings (nibble
// popcount).  Returns 1.0 on length mismatch / empty.
double hammingHex(const QString &a, const QString &b)
{
    if (a.isEmpty() || a.size() != b.size()) return 1.0;
    static const int kPop[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
    int diff = 0;
    const int n = a.size();
    for (int i = 0; i < n; ++i)
        diff += kPop[(hexVal(a.at(i)) ^ hexVal(b.at(i))) & 0xF];
    return double(diff) / (double(n) * 4.0);
}

// col62 holds "1: 040000-0AFFFF" / "2: 100100-1FFADF, 630100-7FFADF" /
// occasionally an error string.  Pull every START-END hex pair.
QVector<QPair<qint64, qint64>> parseRegions(const QString &col62)
{
    QVector<QPair<qint64, qint64>> out;
    static const QRegularExpression re(
        QStringLiteral("([0-9A-Fa-f]+)\\s*-\\s*([0-9A-Fa-f]+)"));
    auto it = re.globalMatch(col62);
    while (it.hasNext()) {
        const auto m = it.next();
        bool ok1 = false, ok2 = false;
        const qint64 a = m.captured(1).toLongLong(&ok1, 16);
        const qint64 b = m.captured(2).toLongLong(&ok2, 16);
        if (ok1 && ok2 && b >= a) out.append({a, b});
    }
    return out;
}

}  // namespace

WolsTwinFinder::WolsTwinFinder()
{
    Config cfg;
    m_dbs = cfg.discoverCacheDbs();
    QString err;
    m_storeOk = m_store.open(&err);
    if (!m_storeOk)
        qCWarning(catFind) << "WolsTwinFinder: extract store open failed:" << err;
}

bool WolsTwinFinder::storePopulated() const
{
    return m_storeOk && m_store.isPopulated();
}

SyncStats WolsTwinFinder::sync(
    const std::function<void(int, int, const QString &)> &progress)
{
    if (!m_storeOk) return {};
    return m_store.sync(m_dbs, progress);
}

QString WolsTwinFinder::crc32Identity(const QByteArray &region)
{
    uLong c = ::crc32(0L, Z_NULL, 0);
    const Bytef *p = reinterpret_cast<const Bytef *>(region.constData());
    qint64 remaining = region.size();
    while (remaining > 0) {
        const uInt chunk = uInt(qMin<qint64>(remaining, qint64(1) << 30));
        c = ::crc32(c, p, chunk);
        p += chunk;
        remaining -= chunk;
    }
    const quint32 raw   = quint32(c) ^ 0xFFFFFFFFu;   // undo final inversion
    const quint32 top24 = (raw >> 8) & 0xFFFFFFu;     // first 3 bytes WinOLS keeps
    return QString::asprintf("%06x", top24);
}

QVector<ExactTwin> WolsTwinFinder::find(const QByteArray &rom,
                                        QPair<qint64, qint64> *dataRegion) const
{
    QVector<ExactTwin> result;
    if (dataRegion) *dataRegion = {-1, -1};
    if (rom.isEmpty() || !m_storeOk) return result;

    const QString idProject = crc32Identity(rom);
    qCInfo(catFind) << "WolsTwinFinder(store): idProject=" << idProject;

    QHash<QString, int> idxByKey;
    auto mergeEntry = [&](const StoreEntry &e, bool proj, bool data) {
        const QString key = e.sourceDb + QLatin1Char('|') + e.filename;
        int i = idxByKey.value(key, -1);
        if (i < 0) {
            ExactTwin t;
            t.dbBasename = e.sourceDb;  t.filename     = e.filename;
            t.make       = e.make;      t.model        = e.model;
            t.ecuModel   = e.ecuModel;  t.swNumber     = e.swNumber;
            t.winolsNumber = e.winolsNumber; t.regions = e.regions;
            result.append(t);
            i = result.size() - 1;
            idxByKey.insert(key, i);
        }
        if (proj) result[i].projectTwin = true;
        if (data) result[i].dataTwin    = true;
    };

    QString needleC0, needleC1;
    QPair<qint64, qint64> needleRegion(-1, -1);

    // ── Phase 1: whole-file (project) twins — indexed lookup ─────────────
    QVector<QPair<qint64, qint64>> regionList;
    QSet<QString> regionSeen;
    for (const StoreEntry &e : m_store.byIdProject(idProject)) {
        mergeEntry(e, /*proj*/ true, /*data*/ true);
        if (needleC0.isEmpty()) { needleC0 = e.chunk0; needleC1 = e.chunk1; }
        for (const auto &pr : parseRegions(e.regions)) {
            const QString k = QString::number(pr.first) + QLatin1Char(':')
                            + QString::number(pr.second);
            if (!regionSeen.contains(k)) {
                regionSeen.insert(k);
                regionList.append(pr);
                if (needleRegion.first < 0) needleRegion = pr;
            }
        }
    }

    // ── Phase 2: data-area twins — indexed lookup over derived idData ────
    QStringList idDatas;
    QSet<QString> idDataSet;
    for (const auto &pr : regionList) {
        const qint64 a = pr.first;
        if (a < 0 || a >= rom.size()) continue;
        const qint64 len = qMin<qint64>(pr.second - a + 1, rom.size() - a);
        if (len <= 0) continue;
        const QString id = crc32Identity(rom.mid(int(a), int(len)));
        if (!idDataSet.contains(id)) { idDataSet.insert(id); idDatas << id; }
    }
    constexpr int kDataCap = 300;
    int dataCount = 0, dataTotal = 0;
    if (!idDatas.isEmpty()) {
        for (const StoreEntry &e : m_store.byIdData(idDatas)) {
            if (!idDataSet.contains(e.idData)) continue;
            ++dataTotal;
            if (needleC1.isEmpty()) needleC1 = e.chunk1;
            if (needleRegion.first < 0) {
                const auto prs = parseRegions(e.regions);
                if (!prs.isEmpty()) needleRegion = prs.first();
            }
            if (dataCount < kDataCap) { mergeEntry(e, false, true); ++dataCount; }
        }
    }
    if (dataRegion) *dataRegion = needleRegion;
    qCInfo(catFind) << "WolsTwinFinder(store): exact twins=" << result.size()
                    << "(data total" << dataTotal << ")";

    // ── Phase 3: fuzzy candidates — scan the local extract ───────────────
    if (!needleC1.isEmpty() || !needleC0.isEmpty()) {
        constexpr double kThr = 0.33;
        QVector<ExactTwin> sims;
        m_store.forEachEntry([&](const StoreEntry &e) {
            if (idxByKey.contains(e.sourceDb + QLatin1Char('|') + e.filename))
                return;
            const double dd = needleC1.isEmpty() ? 1.0 : hammingHex(e.chunk1, needleC1);
            const double pd = needleC0.isEmpty() ? 1.0 : hammingHex(e.chunk0, needleC0);
            const double best = qMin(dd, pd);
            if (best > 0.0 && best <= kThr) {
                ExactTwin t;
                t.dbBasename = e.sourceDb;  t.filename     = e.filename;
                t.make       = e.make;      t.model        = e.model;
                t.ecuModel   = e.ecuModel;  t.swNumber     = e.swNumber;
                t.winolsNumber = e.winolsNumber; t.regions  = e.regions;
                t.similar = true;  t.dataDist = dd;  t.projDist = pd;
                sims.append(t);
            }
        });
        std::sort(sims.begin(), sims.end(), [](const ExactTwin &a, const ExactTwin &b) {
            return qMin(a.dataDist, a.projDist) < qMin(b.dataDist, b.projDist);
        });
        constexpr int kCap = 200;
        if (sims.size() > kCap) sims.resize(kCap);
        result += sims;
        qCInfo(catFind) << "WolsTwinFinder(store): fuzzy candidates=" << sims.size();
    }

    // ── Resolve on-disk paths (fast fileRoots only; scanFallback is lazy) ─
    {
        Config cfg;
        const auto fileRoots = cfg.fileRoots();
        for (ExactTwin &t : result) {
            auto fr = fileRoots.constFind(t.dbBasename);
            if (fr != fileRoots.constEnd()) {
                const QString cand = QDir(fr.value()).filePath(t.filename);
                if (QFileInfo::exists(cand)) t.path = cand;
            }
        }
    }

    // Order: exact project twins, then exact data twins, then fuzzy by dist.
    auto rankOf = [](const ExactTwin &t) {
        if (t.similar) return 2;
        return t.projectTwin ? 0 : 1;
    };
    std::stable_sort(result.begin(), result.end(),
                     [&](const ExactTwin &a, const ExactTwin &b) {
        const int ra = rankOf(a), rb = rankOf(b);
        if (ra != rb) return ra < rb;
        if (ra == 2) return qMin(a.dataDist, a.projDist) < qMin(b.dataDist, b.projDist);
        return false;
    });
    return result;
}

}  // namespace winols
