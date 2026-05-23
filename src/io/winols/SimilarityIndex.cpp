/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/SimilarityIndex.h"
#include "io/winols/WinOlsConfig.h"
#include "io/ols/OlsImporter.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <QtConcurrent>

#include <algorithm>

namespace winols {

namespace {

// Read the file and produce a RomFingerprint.  For .ols / .kp containers
// we MUST unwrap to the inner ROM bytes — the same ROM lives in many
// different .ols files (each with its own header, project name, version
// list, map TLVs).  Hashing the raw container bytes makes two .ols files
// containing the SAME ROM look completely different, which defeats the
// whole point of similarity search.
//
// For multi-version .ols (Original + Stage 1 + …), we score against ALL
// versions and store the per-bucket MIN MinHash slot across them — this
// is the sketch of the union of all versions' shingles, and it lets the
// file match if ANY of its versions is similar to the needle.  (The
// alternative — pick versions[0] only — would miss matches against
// tuned variants when the .ols's "Original" version differs from the
// needle.)
//
// Fallback: if the OLS parse fails, hash the raw bytes (better than
// dropping the file entirely; a corrupted .ols still has *some* signal).
// Fingerprint a file as one row PER VERSION (not a merged "union" sketch).
// Separate per-version records give the query an honest, precise score and
// let it report which version actually matched (also helps Catalog Tune
// Suggestions).  Raw .bin/.ori → a single version (index 0, empty label).
QList<IndexedFile> hashAllVersions(const QString &path)
{
    QList<IndexedFile> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;
    constexpr qint64 kMaxRead = 64 * 1024 * 1024;     // 64 MB cap (.ols can grow)
    QByteArray fileData = f.read(kMaxRead);
    f.close();
    if (fileData.size() < 4096) return out;

    QFileInfo fi(path);
    const qint64 size  = fi.size();
    const qint64 mtime = fi.lastModified().toSecsSinceEpoch();
    auto mk = [&](int vi, const QString &label, const RomFingerprint &fp) {
        IndexedFile r;
        r.path = path; r.size = size; r.mtime = mtime;
        r.versionIndex = vi; r.versionLabel = label;
        r.fp = fp; r.bytesScanned = fp.bytesScanned;
        return r;
    };

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("ols") || ext == QStringLiteral("kp")) {
        const ols::OlsImportResult res = ols::OlsImporter::importFromBytes(fileData);
        if (res.error.isEmpty() && !res.versions.isEmpty()) {
            int vi = 0;
            for (const auto &v : res.versions) {
                if (v.romData.size() >= kShingleSize) {
                    const RomFingerprint fp = fingerprint(QByteArrayView(v.romData));
                    if (!fp.isEmpty()) out.append(mk(vi, v.name, fp));
                }
                ++vi;
            }
            if (!out.isEmpty()) return out;
        }
        // Parse failed — fall through to raw-bytes fingerprint.
    }
    const RomFingerprint fp = fingerprint(QByteArrayView(fileData));
    if (!fp.isEmpty()) out.append(mk(0, QString(), fp));
    return out;
}

}  // namespace

SimilarityIndex::SimilarityIndex(QObject *parent)
    : QObject(parent),
      m_connectionName(QStringLiteral("similarity_idx_") +
                       QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

SimilarityIndex::~SimilarityIndex() { close(); }

bool SimilarityIndex::open(QString *err)
{
    close();
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (err) *err = QStringLiteral("Qt SQLITE driver missing");
        return false;
    }
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath();
    QDir().mkpath(base);
    m_dbPath = QDir(base).filePath(QStringLiteral("similarity_index.db"));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        if (err) *err = QStringLiteral("open failed: %1")
                            .arg(db.lastError().text());
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    if (!createSchemaIfNeeded(db, err)) {
        db.close();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    m_open = true;
    return true;
}

void SimilarityIndex::close()
{
    if (!m_open) return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) db.close();
    }  // db copy goes out of scope before removeDatabase
    QSqlDatabase::removeDatabase(m_connectionName);
    m_open = false;
}

bool SimilarityIndex::createSchemaIfNeeded(QSqlDatabase &db, QString *err) const
{
    QSqlQuery q(db);
    // One row PER (file, version).  `postings` is the inverted MinHash index
    // (one row per non-empty bucket hash) so findSimilar() only touches
    // candidates that actually share a min-hash with the needle.
    const char *ddl[] = {
        "CREATE TABLE IF NOT EXISTS files ("
        "  file_id       INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  path          TEXT,"
        "  version_index INTEGER,"
        "  version_label TEXT,"
        "  size          INTEGER,"
        "  mtime         INTEGER,"
        "  bytes_scanned INTEGER,"
        "  blob          BLOB)",
        "CREATE INDEX IF NOT EXISTS idx_files_path ON files(path)",
        "CREATE TABLE IF NOT EXISTS postings (hash INTEGER, file_id INTEGER)",
        "CREATE INDEX IF NOT EXISTS idx_postings_hash ON postings(hash)",
    };
    for (const char *s : ddl) {
        if (!q.exec(QLatin1String(s))) {
            if (err) *err = q.lastError().text();
            return false;
        }
    }
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    return true;
}

int SimilarityIndex::rowCount() const
{
    if (!m_open) return 0;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM files")) && q.next())
        return q.value(0).toInt();
    return 0;
}

qint64 SimilarityIndex::insertFile(QSqlDatabase &db, const IndexedFile &f) const
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO files(path,version_index,version_label,size,mtime,"
        "bytes_scanned,blob) VALUES(?,?,?,?,?,?,?)"));
    q.addBindValue(f.path);
    q.addBindValue(f.versionIndex);
    q.addBindValue(f.versionLabel);
    q.addBindValue(f.size);
    q.addBindValue(f.mtime);
    q.addBindValue(f.bytesScanned);
    q.addBindValue(f.fp.toBlob());
    if (!q.exec()) return -1;
    const qint64 fid = q.lastInsertId().toLongLong();

    // Inverted index: one posting per non-empty MinHash bucket.
    QSqlQuery p(db);
    p.prepare(QStringLiteral("INSERT INTO postings(hash,file_id) VALUES(?,?)"));
    for (quint64 h : f.fp.wholeFile) {
        if (h == 0) continue;   // empty bucket sentinel
        p.bindValue(0, QVariant::fromValue<qulonglong>(h));
        p.bindValue(1, fid);
        p.exec();
    }
    return fid;
}

QStringList SimilarityIndex::enumerate(const QStringList &roots)
{
    static const QStringList kSuffixes = {
        QStringLiteral("*.ols"), QStringLiteral("*.kp"),
        QStringLiteral("*.bin"), QStringLiteral("*.rom"),
        QStringLiteral("*.ori"),
    };
    QStringList out;
    for (const QString &r : roots) {
        QDir d(r);
        if (!d.exists()) continue;
        QDirIterator it(r, kSuffixes, QDir::Files | QDir::Readable,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) out << it.next();
    }
    return out;
}

void SimilarityIndex::rebuild(const QStringList &roots)
{
    if (!m_open) {
        emit rebuildFinished(0, false);
        return;
    }
    m_cancel.store(false);
    m_paused.store(false);

    QElapsedTimer wall;
    wall.start();
    QStringList paths = enumerate(roots);
    const int total = paths.size();
    if (total == 0) {
        emit rebuildFinished(0, false);
        return;
    }

    // Qt's SQLITE driver requires that a connection is used only on
    // the thread that opened it.  rebuild() runs on a worker thread
    // (QtConcurrent::run from BuildIndexProgressDlg), so the
    // connection opened on the main thread (`m_connectionName`) is
    // off-limits here.  Open a private connection scoped to this call.
    const QString workerConn = m_connectionName +
                               QStringLiteral("_worker_") +
                               QString::number(qint64(QThread::currentThreadId()));
    {
        QSqlDatabase wdb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                     workerConn);
        wdb.setDatabaseName(m_dbPath);
        if (!wdb.open()) {
            QSqlDatabase::removeDatabase(workerConn);
            emit rebuildFinished(0, false);
            return;
        }
        QSqlQuery q(wdb);
        q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    }
    QSqlDatabase db = QSqlDatabase::database(workerConn);

    // Purge rows whose blob is NOT the current fingerprint format.
    // When the algorithm changes (RFP1 → RFP2 → RFP3 …) old rows would
    // otherwise be skipped by the (size,mtime) pre-filter below and
    // never re-fingerprinted.  Run-once cost; rows with current magic
    // survive untouched so incremental rebuilds stay cheap.
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "DELETE FROM files WHERE SUBSTR(blob,1,4) != 'RFP4'"))) {
            const int purged = q.numRowsAffected();
            if (purged > 0)
                qWarning("SimilarityIndex: purged %d rows in old fingerprint "
                         "format — they will be re-fingerprinted now.", purged);
        }
        // Drop postings orphaned by the purge (or any prior crash).
        q.exec(QStringLiteral(
            "DELETE FROM postings WHERE file_id NOT IN (SELECT file_id FROM files)"));
    }

    // Pre-load existing path → (size,mtime) into memory for O(1)
    // skip lookup.  ~16 bytes per row, fits 100k+ in <2 MB.
    QHash<QString, QPair<qint64, qint64>> known;
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT path,size,mtime FROM files"))) {
            while (q.next())
                known.insert(q.value(0).toString(),
                             {q.value(1).toLongLong(), q.value(2).toLongLong()});
        }
    }

    int processed = 0;
    qint64 totalBytes = 0;
    // Smaller batches mean the user sees `rowCount` climbing in real
    // time (visible in DB tools, find-similar query results, etc.)
    // instead of waiting for the next 200-file commit.  Throughput
    // cost is negligible — SQLite WAL handles small TX cheaply.
    constexpr int kCommitEvery = 50;

    // Pre-filter: split paths into "skip" (already indexed, identical
    // size+mtime) and "fresh" (need fingerprinting).  Doing this up
    // front lets the parallel pool spend 100 % of its time on real
    // work instead of stat+hashmap-lookup serialised through one core.
    QStringList fresh;
    fresh.reserve(paths.size());
    for (const QString &path : paths) {
        QFileInfo fi(path);
        const qint64 size  = fi.size();
        const qint64 mtime = fi.lastModified().toSecsSinceEpoch();
        auto it = known.find(path);
        if (it != known.end()
            && it.value().first == size
            && it.value().second == mtime) {
            ++processed;
            continue;
        }
        fresh.append(path);
    }
    if (processed > 0)
        emit progress(processed, total, totalBytes, wall.elapsed(), QString());

    // Multi-threaded fingerprinting.  hashOne() is pure (no shared state)
    // and dominated by file I/O + Blake3 hashing — both scale almost
    // linearly with cores until disk bandwidth saturates.  We process
    // in chunks so cancel/pause stay responsive (≤ chunk worth of work
    // before checking flags) and so memory stays bounded (chunk × ~16
    // KB per fingerprint, plus the file bytes briefly held by hashOne).
    constexpr int kChunkSize = 64;

    db.transaction();
    int batchInTx = 0;

    // Reused across the loop (prepared statements persist across commits).
    QSqlQuery delP(db);
    delP.prepare(QStringLiteral(
        "DELETE FROM postings WHERE file_id IN "
        "(SELECT file_id FROM files WHERE path=?)"));
    QSqlQuery delF(db);
    delF.prepare(QStringLiteral("DELETE FROM files WHERE path=?"));

    for (int i = 0; i < fresh.size(); i += kChunkSize) {
        while (m_paused.load()) QThread::msleep(50);
        if (m_cancel.load()) break;

        const QStringList chunk = fresh.mid(i, kChunkSize);

        // Fingerprint this chunk in parallel — each path yields one row per
        // version.  Returns once every result is ready.
        const QList<QList<IndexedFile>> results =
            QtConcurrent::blockingMapped<QList<QList<IndexedFile>>>(
                chunk,
                [](const QString &path) { return hashAllVersions(path); });

        // Serial SQLite write: replace each path's rows, insert versions +
        // postings, commit in batches to bound crash recovery.
        for (const QList<IndexedFile> &versions : results) {
            ++processed;
            QString curPath;
            if (!versions.isEmpty()) {
                curPath = versions.first().path;
                totalBytes += versions.first().size;
                delP.bindValue(0, curPath); delP.exec();
                delF.bindValue(0, curPath); delF.exec();
                for (const IndexedFile &f : versions) {
                    insertFile(db, f);
                    ++batchInTx;
                }
            }
            if (batchInTx >= kCommitEvery) {
                db.commit();
                db.transaction();
                batchInTx = 0;
            }
            emit progress(processed, total, totalBytes, wall.elapsed(), curPath);
        }
    }
    db.commit();
    emit progress(processed, total, totalBytes, wall.elapsed(), QString());
    // Close + drop the worker-thread connection before returning so
    // the next rebuild() call can re-create it cleanly.
    db.close();
    QSqlDatabase::removeDatabase(workerConn);
    emit rebuildFinished(processed, m_cancel.load());
}

QVector<SimilarityMatch> SimilarityIndex::findSimilar(
    const RomFingerprint &needle, int minPercent, int limit) const
{
    QVector<SimilarityMatch> out;
    if (!m_open || needle.isEmpty()) return out;

    // Needle's non-empty MinHash bucket values.
    QVector<qulonglong> hashes;
    for (quint64 h : needle.wholeFile) if (h != 0) hashes.append(h);
    if (hashes.isEmpty()) return out;

    const bool onWorker = (QThread::currentThread() != qApp->thread());
    const QString connName = onWorker
        ? m_connectionName + QStringLiteral("_query_") +
          QString::number(reinterpret_cast<quintptr>(QThread::currentThread()), 16)
        : m_connectionName;

    QHash<QString, SimilarityMatch> bestByPath;   // dedup .ols versions by file
    {
        QSqlDatabase db;
        if (onWorker) {
            db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(m_dbPath);
            if (!db.open()) {
                QSqlDatabase::removeDatabase(connName);
                return out;
            }
        } else {
            db = QSqlDatabase::database(m_connectionName);
        }

        // 1. Inverted index: candidates that share ≥1 min-hash with the
        //    needle, ranked by shared-hash count (most similar first).  Only
        //    these get loaded + scored — not the whole table.
        QStringList ph;
        ph.reserve(hashes.size());
        for (int i = 0; i < hashes.size(); ++i) ph << QStringLiteral("?");
        QVector<qint64> cands;
        {
            QSqlQuery q(db);
            q.setForwardOnly(true);
            q.prepare(QStringLiteral(
                "SELECT file_id, COUNT(*) c FROM postings WHERE hash IN (%1) "
                "GROUP BY file_id ORDER BY c DESC LIMIT 6000")
                    .arg(ph.join(QLatin1Char(','))));
            for (qulonglong h : hashes) q.addBindValue(QVariant::fromValue(h));
            if (q.exec()) while (q.next()) cands.append(q.value(0).toLongLong());
        }

        // 2. Exact score of each candidate (load only its blob); keep the
        //    best-scoring version per file path.
        const double threshold = double(minPercent) / 100.0;
        QSqlQuery r(db);
        r.setForwardOnly(true);
        r.prepare(QStringLiteral(
            "SELECT path,size,version_label,blob FROM files WHERE file_id=?"));
        for (qint64 fid : cands) {
            r.bindValue(0, fid);
            if (!r.exec() || !r.next()) continue;
            const RomFingerprint fp = RomFingerprint::fromBlob(r.value(3).toByteArray());
            const SimilarityScore s = winols::similarity(needle, fp);
            if (s.best() < threshold) continue;
            const QString path = r.value(0).toString();
            auto it = bestByPath.find(path);
            if (it == bestByPath.end() || s.best() > it.value().score.best()) {
                SimilarityMatch m;
                m.path         = path;
                m.size         = r.value(1).toLongLong();
                m.versionLabel = r.value(2).toString();
                m.score        = s;
                bestByPath.insert(path, m);
            }
        }
        if (onWorker) db.close();
    }
    if (onWorker)
        QSqlDatabase::removeDatabase(connName);

    out.reserve(bestByPath.size());
    for (auto it = bestByPath.constBegin(); it != bestByPath.constEnd(); ++it)
        out.append(it.value());
    std::sort(out.begin(), out.end(),
              [](const SimilarityMatch &a, const SimilarityMatch &b) {
                  return a.score.best() > b.score.best();
              });
    if (out.size() > limit) out.resize(limit);
    return out;
}

IndexedFile SimilarityIndex::lookup(const QString &path) const
{
    IndexedFile f;
    if (!m_open) return f;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT path,size,mtime,bytes_scanned,blob FROM files "
        "WHERE path=:p"));
    q.bindValue(QStringLiteral(":p"), path);
    if (q.exec() && q.next()) {
        f.path  = q.value(0).toString();
        f.size  = q.value(1).toLongLong();
        f.mtime = q.value(2).toLongLong();
        f.bytesScanned = q.value(3).toLongLong();
        f.fp    = RomFingerprint::fromBlob(q.value(4).toByteArray());
    }
    return f;
}

}  // namespace winols
