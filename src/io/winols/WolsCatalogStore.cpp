/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/WolsCatalogStore.h"

#include "debug/DebugLog.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

namespace winols {

namespace {

QString defaultDbPath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath();
    // AppDataLocation already ends in .../CT14/romHEX14 when org/app are set.
    return QDir(base).filePath(QStringLiteral("wols_catalog.db"));
}

// colx4 = {{0,0,{<HEX0>}},{1,0,{<HEX1>}}...} — the chunk bodies are the only
// long-hex runs; pull the first two.
void extractChunks(const QString &colx4, QString &c0, QString &c1)
{
    static const QRegularExpression re(QStringLiteral("[0-9A-Fa-f]{32,}"));
    auto it = re.globalMatch(colx4);
    int n = 0;
    while (it.hasNext() && n < 2) {
        const QString m = it.next().captured(0);
        if (n == 0) c0 = m; else c1 = m;
        ++n;
    }
}

QString decodeCell(const QVariant &v)
{
    if (v.isNull()) return {};
    const QString s = v.toString();
    if (!s.contains(QChar(0xFFFD))) return s;
    return QString::fromLatin1(v.toByteArray());
}

// Read LastMod2 (WinOLS bumps it on every catalog rewrite).  0 if missing.
qint64 readLastMod(QSqlDatabase &db)
{
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT value FROM nosql_int WHERE id='LastMod2'"))
        && q.next())
        return q.value(0).toLongLong();
    return 0;
}

// Open a source Cache_*.db read-only/immutable and extract the rows we keep.
// Returns false (and leaves *out untouched) if the DB can't be read — the
// caller then keeps whatever was previously extracted (format-change safety).
bool readSource(const QString &dbPath, const QString &sourceDb,
                QVector<StoreEntry> &out, qint64 *lastMod, QString *err)
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (err) *err = QStringLiteral("no QSQLITE driver");
        return false;
    }
    const QString conn = QStringLiteral("wolssync_") +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_OPEN_URI"));
        db.setDatabaseName(QStringLiteral("file:%1?mode=ro&immutable=1")
                               .arg(QFileInfo(dbPath).absoluteFilePath()));
        if (db.open()) {
            if (lastMod) *lastMod = readLastMod(db);
            QSqlQuery q(db);
            q.setForwardOnly(true);
            const QString full = QStringLiteral(
                "SELECT col30,col2,col3,col17,col18,col20,col22,col62,colx4 "
                "FROM dir WHERE colx4 IS NOT NULL");
            if (q.exec(full)) {
                out.reserve(out.size() + 4096);
                while (q.next()) {
                    StoreEntry e;
                    e.sourceDb     = sourceDb;
                    e.filename     = decodeCell(q.value(0));
                    e.make         = decodeCell(q.value(1));
                    e.model        = decodeCell(q.value(2));
                    e.ecuModel     = decodeCell(q.value(3));
                    e.swNumber     = decodeCell(q.value(4));
                    e.winolsNumber = decodeCell(q.value(5));
                    e.fileSize     = q.value(6).toLongLong();
                    e.regions      = decodeCell(q.value(7));
                    QString c0, c1;
                    extractChunks(q.value(8).toString(), c0, c1);
                    e.chunk0    = c0;
                    e.chunk1    = c1;
                    e.idProject = c0.left(6).toLower();
                    e.idData    = c1.left(6).toLower();
                    out.append(e);
                }
                ok = true;
            } else if (err) {
                *err = q.lastError().text();
            }
            db.close();
        } else if (err) {
            *err = db.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return ok;
}

}  // namespace

WolsCatalogStore::WolsCatalogStore(const QString &dbPath)
    : m_dbPath(dbPath.isEmpty() ? defaultDbPath() : dbPath),
      m_conn(QStringLiteral("wolsstore_") +
             QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

WolsCatalogStore::~WolsCatalogStore()
{
    if (m_open) {
        { QSqlDatabase db = QSqlDatabase::database(m_conn); if (db.isOpen()) db.close(); }
        QSqlDatabase::removeDatabase(m_conn);
    }
}

bool WolsCatalogStore::open(QString *err)
{
    if (m_open) return true;
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (err) *err = QStringLiteral("Qt SQLITE driver missing");
        return false;
    }
    QDir().mkpath(QFileInfo(m_dbPath).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_conn);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        if (err) *err = db.lastError().text();
        QSqlDatabase::removeDatabase(m_conn);
        return false;
    }
    m_open = true;
    return ensureSchema(err);
}

bool WolsCatalogStore::ensureSchema(QString *err)
{
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    QSqlQuery q(db);
    const char *ddl[] = {
        "CREATE TABLE IF NOT EXISTS entries ("
        " id INTEGER PRIMARY KEY, source_db TEXT, filename TEXT,"
        " make TEXT, model TEXT, ecu_model TEXT, sw TEXT, winols_num TEXT,"
        " file_size INTEGER, regions TEXT,"
        " id_project TEXT, id_data TEXT, chunk0 TEXT, chunk1 TEXT)",
        "CREATE INDEX IF NOT EXISTS ix_entries_idproject ON entries(id_project)",
        "CREATE INDEX IF NOT EXISTS ix_entries_iddata    ON entries(id_data)",
        "CREATE TABLE IF NOT EXISTS sources ("
        " source_db TEXT PRIMARY KEY, last_mod INTEGER, mtime INTEGER,"
        " size INTEGER, row_count INTEGER, synced_at INTEGER)",
        "CREATE TABLE IF NOT EXISTS meta (k TEXT PRIMARY KEY, v TEXT)",
    };
    for (const char *s : ddl) {
        if (!q.exec(QLatin1String(s))) {
            if (err) *err = q.lastError().text();
            return false;
        }
    }
    q.exec(QStringLiteral("INSERT OR IGNORE INTO meta(k,v) VALUES('schema','1')"));
    return true;
}

int WolsCatalogStore::entryCount() const
{
    if (!m_open) return 0;
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM entries")) && q.next())
        return q.value(0).toInt();
    return 0;
}

SyncStats WolsCatalogStore::sync(
    const QStringList &sourceCacheDbs,
    const std::function<void(int, int, const QString &)> &progress)
{
    SyncStats st;
    QElapsedTimer timer; timer.start();
    if (!m_open) return st;

    QSqlDatabase db = QSqlDatabase::database(m_conn);
    // Bulk-insert speed; this DB is fully rebuildable so durability is moot.
    { QSqlQuery p(db); p.exec(QStringLiteral("PRAGMA synchronous=OFF"));
                       p.exec(QStringLiteral("PRAGMA journal_mode=MEMORY")); }

    const int total = sourceCacheDbs.size();
    int idx = 0;
    for (const QString &dbPath : sourceCacheDbs) {
        const QString srcName = QFileInfo(dbPath).fileName();
        if (progress) progress(idx, total, srcName);
        ++idx;

        const QFileInfo fi(dbPath);
        const qint64 mtime = fi.lastModified().toSecsSinceEpoch();
        const qint64 size  = fi.size();

        // Skip if unchanged since last sync (same mtime+size).
        {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT mtime,size FROM sources WHERE source_db=?"));
            q.addBindValue(srcName);
            if (q.exec() && q.next() &&
                q.value(0).toLongLong() == mtime && q.value(1).toLongLong() == size) {
                ++st.dbsSkipped;
                continue;
            }
        }

        // Read the source (RO).  On failure, keep whatever we already have.
        QVector<StoreEntry> rows;
        qint64 lastMod = 0;
        QString rerr;
        if (!readSource(dbPath, srcName, rows, &lastMod, &rerr)) {
            qCWarning(catFind) << "WolsCatalogStore: skip" << srcName
                               << "(unreadable —" << rerr << "); keeping prior data";
            ++st.dbsFailed;
            continue;
        }

        // Atomically replace this source's rows.
        if (!db.transaction()) { ++st.dbsFailed; continue; }
        bool okTx = true;
        {
            QSqlQuery del(db);
            del.prepare(QStringLiteral("DELETE FROM entries WHERE source_db=?"));
            del.addBindValue(srcName);
            okTx = del.exec();
        }
        if (okTx) {
            QSqlQuery ins(db);
            ins.prepare(QStringLiteral(
                "INSERT INTO entries(source_db,filename,make,model,ecu_model,sw,"
                "winols_num,file_size,regions,id_project,id_data,chunk0,chunk1) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)"));
            for (const StoreEntry &e : rows) {
                ins.addBindValue(e.sourceDb);   ins.addBindValue(e.filename);
                ins.addBindValue(e.make);       ins.addBindValue(e.model);
                ins.addBindValue(e.ecuModel);   ins.addBindValue(e.swNumber);
                ins.addBindValue(e.winolsNumber); ins.addBindValue(e.fileSize);
                ins.addBindValue(e.regions);    ins.addBindValue(e.idProject);
                ins.addBindValue(e.idData);     ins.addBindValue(e.chunk0);
                ins.addBindValue(e.chunk1);
                if (!ins.exec()) { okTx = false; break; }
            }
        }
        if (okTx) {
            QSqlQuery up(db);
            up.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO sources(source_db,last_mod,mtime,size,"
                "row_count,synced_at) VALUES(?,?,?,?,?,?)"));
            up.addBindValue(srcName);   up.addBindValue(lastMod);
            up.addBindValue(mtime);     up.addBindValue(size);
            up.addBindValue(int(rows.size()));
            up.addBindValue(QDateTime::currentSecsSinceEpoch());
            okTx = up.exec();
        }
        if (okTx && db.commit()) {
            ++st.dbsScanned;
            st.rowsExtracted += rows.size();
        } else {
            db.rollback();
            ++st.dbsFailed;
            qCWarning(catFind) << "WolsCatalogStore: tx failed for" << srcName;
        }
    }
    if (progress) progress(total, total, QString());

    {
        QSqlQuery m(db);
        m.prepare(QStringLiteral("INSERT OR REPLACE INTO meta(k,v) VALUES('last_sync',?)"));
        m.addBindValue(QString::number(QDateTime::currentSecsSinceEpoch()));
        m.exec();
    }
    st.rowsTotal = entryCount();
    st.elapsedMs = timer.elapsed();
    qCInfo(catFind) << "WolsCatalogStore sync: scanned" << st.dbsScanned
                    << "skipped" << st.dbsSkipped << "failed" << st.dbsFailed
                    << "rows+" << st.rowsExtracted << "total" << st.rowsTotal
                    << "in" << st.elapsedMs << "ms";
    return st;
}

namespace {
StoreEntry rowToEntry(const QSqlQuery &q)
{
    StoreEntry e;
    e.sourceDb     = q.value(0).toString();
    e.filename     = q.value(1).toString();
    e.make         = q.value(2).toString();
    e.model        = q.value(3).toString();
    e.ecuModel     = q.value(4).toString();
    e.swNumber     = q.value(5).toString();
    e.winolsNumber = q.value(6).toString();
    e.fileSize     = q.value(7).toLongLong();
    e.regions      = q.value(8).toString();
    e.idProject    = q.value(9).toString();
    e.idData       = q.value(10).toString();
    e.chunk0       = q.value(11).toString();
    e.chunk1       = q.value(12).toString();
    return e;
}
const char *kSelectCols =
    "source_db,filename,make,model,ecu_model,sw,winols_num,file_size,"
    "regions,id_project,id_data,chunk0,chunk1";
}  // namespace

QVector<StoreEntry> WolsCatalogStore::byIdProject(const QString &id6) const
{
    QVector<StoreEntry> out;
    if (!m_open || id6.isEmpty()) return out;
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT %1 FROM entries WHERE id_project=?")
                  .arg(QLatin1String(kSelectCols)));
    q.addBindValue(id6.toLower());
    if (q.exec()) while (q.next()) out.append(rowToEntry(q));
    return out;
}

QVector<StoreEntry> WolsCatalogStore::byIdData(const QStringList &id6s) const
{
    QVector<StoreEntry> out;
    if (!m_open || id6s.isEmpty()) return out;
    QStringList ph;
    for (int i = 0; i < id6s.size(); ++i) ph << QStringLiteral("?");
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT %1 FROM entries WHERE id_data IN (%2)")
                  .arg(QLatin1String(kSelectCols), ph.join(QLatin1Char(','))));
    for (const QString &id : id6s) q.addBindValue(id.toLower());
    if (q.exec()) while (q.next()) out.append(rowToEntry(q));
    return out;
}

void WolsCatalogStore::forEachEntry(
    const std::function<void(const StoreEntry &)> &cb) const
{
    if (!m_open || !cb) return;
    QSqlDatabase db = QSqlDatabase::database(m_conn);
    QSqlQuery q(db);
    q.setForwardOnly(true);
    if (q.exec(QStringLiteral("SELECT %1 FROM entries").arg(QLatin1String(kSelectCols))))
        while (q.next()) cb(rowToEntry(q));
}

}  // namespace winols
