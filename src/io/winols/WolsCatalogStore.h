/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Local, format-independent extract of the WinOLS catalog.
 * =========================================================
 *
 * WinOLS keeps its catalog in `Cache_*.db` files whose layout we reverse-
 * engineered (colx4 fingerprints etc.).  Reading those live on every query
 * couples us to WinOLS's format and keeps touching its files.  This store
 * is our own SQLite database (`wols_catalog.db`) that holds *only the
 * extracted fields we need*, in a schema we control:
 *
 *   - identity:  id_project / id_data  (top-24 CRC32 of chunk0 / chunk1)
 *   - bodies:    chunk0 / chunk1        (68-byte fingerprint bodies, hex)
 *   - regions:   col62 data-region map
 *   - metadata:  make / model / ecu / sw / winols# / size / filename / db
 *
 * Once `sync()` has run, all similarity lookups read from here — fast
 * (indexed on id_project / id_data) and independent of the WinOLS format.
 *
 * Resilience: sync() is incremental (per source `Cache_*.db`, keyed on its
 * LastMod2 + file mtime/size) and NEVER deletes the extract on a failed
 * read.  If a future WinOLS version changes the format so we can no longer
 * parse a source DB, that source is skipped and whatever we already
 * extracted stays usable.  The source files are opened read-only
 * (`mode=ro&immutable=1`) and only during sync.
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace winols {

struct StoreEntry {
    QString sourceDb;       // Cache_*.db basename
    QString filename;       // col30
    QString make;           // col2
    QString model;          // col3
    QString ecuModel;       // col17
    QString swNumber;       // col18
    QString winolsNumber;   // col20
    QString regions;        // col62
    QString idProject;      // chunk0[:6] lowercase (top-24 CRC32)
    QString idData;         // chunk1[:6] lowercase
    QString chunk0;         // whole-file fingerprint body (hex), may be empty
    QString chunk1;         // data-area fingerprint body (hex), may be empty
    qint64  fileSize = 0;   // col22
};

struct SyncStats {
    int dbsScanned   = 0;   // source DBs (re)extracted this run
    int dbsSkipped   = 0;   // unchanged since last sync
    int dbsFailed    = 0;   // could not read (e.g. format change) — kept old data
    int rowsExtracted = 0;  // rows written this run
    int rowsTotal    = 0;   // total rows in the extract afterwards
    qint64 elapsedMs = 0;
};

class WolsCatalogStore {
public:
    /// @p dbPath — extract DB location.  Empty → default
    /// `<AppDataLocation>/CT14/romHEX14/wols_catalog.db`.
    explicit WolsCatalogStore(const QString &dbPath = QString());
    ~WolsCatalogStore();
    WolsCatalogStore(const WolsCatalogStore &) = delete;
    WolsCatalogStore &operator=(const WolsCatalogStore &) = delete;

    QString dbPath() const { return m_dbPath; }

    /// Open (creating + migrating schema as needed).  Sets *err on failure.
    bool open(QString *err = nullptr);
    bool isOpen() const { return m_open; }

    /// Total extracted rows (0 = not yet synced).
    int entryCount() const;
    bool isPopulated() const { return entryCount() > 0; }

    /// Re-extract from the given source `Cache_*.db` paths (read-only).
    /// Incremental + crash/format safe — see file header.  @p progress is
    /// called as (dbDone, dbTotal, currentDbName).
    SyncStats sync(const QStringList &sourceCacheDbs,
                   const std::function<void(int, int, const QString &)> &progress = {});

    // ── Queries (used by WolsTwinFinder) ──────────────────────────────────
    /// Exact whole-file twins: entries whose id_project == @p id6.
    QVector<StoreEntry> byIdProject(const QString &id6) const;
    /// Exact data-area twins: entries whose id_data is in @p id6s.
    QVector<StoreEntry> byIdData(const QStringList &id6s) const;
    /// Stream every entry (for fuzzy ranking).  Forward-only.
    void forEachEntry(const std::function<void(const StoreEntry &)> &cb) const;

private:
    QString m_dbPath;
    QString m_conn;
    bool    m_open = false;

    bool ensureSchema(QString *err);
};

}  // namespace winols
