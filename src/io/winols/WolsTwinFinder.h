/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Exact-twin finder over the WinOLS Cache_*.db catalog — no import.
 * ================================================================
 *
 * WinOLS stores, per project, a `colx4` fingerprint whose first three
 * bytes are the top-24 bits of CRC32(region) (reflected poly 0xEDB88320,
 * init 0xFFFFFFFF, NO final XOR).  That identity is fully reproducible
 * from raw ROM bytes — so we can find a needle's byte-identical twins
 * directly in the catalog, without building our own MinHash index.
 *
 *   chunk0 = whole-file fingerprint  → WinOLS "% Project"
 *   chunk1 = data-region fingerprint → WinOLS "% Data area"
 *
 * Strategy (deterministic, zero false positives in practice):
 *   1. idProject = crc32Identity(whole file).  Catalog rows whose
 *      chunk0 starts with idProject are whole-file twins (100% project),
 *      which are necessarily data-area twins too.
 *   2. For the data region we need WinOLS's own region boundaries
 *      (col62).  A project twin is byte-identical to the needle, so its
 *      col62 region applied to the needle yields the needle's data
 *      region.  idData = crc32Identity(needle[region]); catalog rows
 *      whose chunk1 starts with idData are data-area twins (100% data) —
 *      this surfaces cross-file twins (same calibration, different whole
 *      file) the way WinOLS's "identical" list does.
 *
 * The exact per-segment fingerprint body and sub-100% similarity are NOT
 * reproduced here (the body hash is proprietary); this finder is the
 * deterministic exact-twin half ("Tryb A").  Read-only: every catalog
 * DB is opened `mode=ro&immutable=1`.
 */

#pragma once

#include "io/winols/WolsCatalogStore.h"

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace winols {

struct ExactTwin {
    QString dbBasename;     // Cache_*.db basename (key for path resolution)
    QString filename;       // col30
    QString path;           // resolved on-disk path, or empty if unresolved
    QString make;           // col2
    QString model;          // col3
    QString ecuModel;       // col17
    QString swNumber;       // col18
    QString winolsNumber;   // col20
    QString regions;        // col62 (data region map text)
    bool    projectTwin = false;  // chunk0 == idProject (whole file identical)
    bool    dataTwin    = false;  // chunk1 == idData    (data area identical)
    // Tryb B: fuzzy candidate (not an exact twin).  Distances are colx4
    // body bit-Hamming to the needle (0 = identical, ~0.5 = unrelated); a
    // coarse pre-filter — the dialog computes the true byte-% afterwards.
    bool    similar     = false;
    double  dataDist    = 1.0;     // chunk1 (data-area) fingerprint distance
    double  projDist    = 1.0;     // chunk0 (whole-file) fingerprint distance
};

class WolsTwinFinder {
public:
    /// Opens the local extract DB (wols_catalog.db) and reads the WinOLS
    /// source-cache list from config.  Safe to construct + use on a worker
    /// thread (its SQLite connection is created here and used only here).
    WolsTwinFinder();

    /// True once the local extract has been populated by sync().
    bool storePopulated() const;

    /// (Re)extract the WinOLS catalog into our local DB (reads source
    /// Cache_*.db read-only; incremental + format-change safe).  Run on a
    /// worker thread.  @p progress is (dbDone, dbTotal, currentDbName).
    SyncStats sync(const std::function<void(int, int, const QString &)> &progress = {});

    /// Find byte-identical twins AND fuzzy similar candidates of @p romBytes,
    /// reading the LOCAL extract DB (independent of the WinOLS format).
    /// Exact twins first, then fuzzy candidates (only when an exact twin in
    /// the catalog supplies the needle's own colx4 body).  @p dataRegion
    /// (out) receives the matched twin's [start,end] data region.
    QVector<ExactTwin> find(const QByteArray &romBytes,
                            QPair<qint64, qint64> *dataRegion = nullptr) const;

    /// 6-hex-char lowercase identity = top 24 bits of
    /// (CRC32(region) XOR 0xFFFFFFFF).  Matches the first 3 bytes WinOLS
    /// stores at the start of each colx4 chunk.
    static QString crc32Identity(const QByteArray &region);

    /// Number of WinOLS source cache DBs discovered (for sync/diagnostics).
    int cacheDbCount() const { return m_dbs.size(); }

private:
    QStringList      m_dbs;     // source Cache_*.db paths (for sync)
    WolsCatalogStore m_store;   // our local extract DB
    bool             m_storeOk = false;
};

}  // namespace winols
