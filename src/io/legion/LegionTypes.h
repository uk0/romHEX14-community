/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * LEGION — crowd-tuned verdicts from the SimilarityIndex catalog.
 *
 * The catalog has thousands of .ols entries with byte-level diffs but no
 * map metadata.  LEGION infers structure (regions, cell-size, dimensions),
 * clusters voices by intent (stage1 vs DPF-off vs EGR-off etc.), and
 * aggregates per-cell deltas into "verdicts" the user can selectively
 * submit to their active project.
 *
 * "We are many." — Mk 5:9.
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>
#include <cstdint>

namespace legion {

// ── Per-voice (per source file) ───────────────────────────────────────────

// Region of contiguous (within K) byte changes detected in one voice.
struct LegionRegion {
    uint32_t   startAddr   = 0;   // inclusive, in user-target address space
    uint32_t   endAddr     = 0;   // inclusive
    QByteArray originalBytes;     // source's Version 0 bytes in this range
    QByteArray modifiedBytes;     // source's Version 1 bytes in this range
};

// One source file's full contribution: its regions + lookup set.
//
// A voice is ONE (Version 0 → Version i) diff from a catalog .ols.  A
// multi-version .ols (Original + Stage 1 + Stage 2 + …) emits one voice per
// tuned version so the UI can attribute each suggestion to a concrete
// version, and the per-voice local-similarity gate can keep only the
// versions whose original actually matches the user's baseline (#1/#2).
struct LegionVoice {
    QString               sourcePath;
    int                   versionIndex = -1;   // .ols version this voice diffs (vs v0)
    QString               versionLabel;        // human label, e.g. "Stage 1"
    int                   similarity = 0;   // overall % from SimilarityIndex
    QVector<LegionRegion> regions;
    QSet<uint32_t>        addressSet;       // every changed byte addr (for Jaccard)
};

// ── Voice clustering by intent (LEGION.3a) ────────────────────────────────

// Auto-detected intent cluster — voices that touch similar address sets.
struct VoiceCluster {
    QVector<int>          voiceIndices;          // indices into voices array
    uint32_t              addrRangeMin   = 0;
    uint32_t              addrRangeMax   = 0;
    int                   consensusAddrCount = 0;   // addrs touched by >=50% members
    QHash<QString, int>   filenameKeywords;         // "stage" -> 11, "dpf" -> 3, ...
    QString               label;                    // human hint, e.g. "stage1-like"
};

// ── Verdicts (cross-file consensus regions) ───────────────────────────────

enum class VerdictKind {
    Scalar    = 0,   // 1-2 cells (limiter, flag)
    Curve     = 1,   // 3-16 cells (1D characteristic)
    SmallMap  = 2,   // 17-100 cells (small 2D)
    LargeMap  = 3,   // >100 cells
};

enum class VerdictTag {
    Unanimous       = 0,   // 100% voices same direction
    StrongConsensus = 1,   // >=80% same direction AND stdDev/mean < 0.2
    Majority        = 2,   // 50-80% same direction
    Contested       = 3,   // <50%, customer-specific noise
    Heretic         = 4,   // <10% of voices, filter out by default
    Checksum        = 5,   // 1-4B in non-tunable area, likely CRC
    KillRegion      = 6,   // meanDelta cancels original — unanimous "disable"
};

// Per-cell aggregated statistics.
struct CellStats {
    double  meanDelta   = 0.0;   // avg of (stage1[i] - original[i]) across voices
    double  stdDevDelta = 0.0;   // std-dev of the delta
    int     sampleCount = 0;     // how many voices contributed to this cell
};

// Canonical region after cross-voice aggregation + classification.
struct LegionVerdict {
    uint32_t              startAddr      = 0;
    uint32_t              endAddr        = 0;
    VerdictKind           kind           = VerdictKind::Scalar;
    VerdictTag            tag            = VerdictTag::Heretic;
    int                   cellSize       = 1;   // 1/2/4/8 bytes per cell
    bool                  bigEndian      = false;   // HiLo if true, else LoHi
    int                   rows           = 1;
    int                   cols           = 1;
    QVector<CellStats>    cells;                // per-cell statistics
    int                   maxSampleCount = 0;   // densest cell's contributor count
    double                consensusStrength = 0.0;   // 0..1, ranking signal
    QVector<int>          contributingVoices;        // indices into voices array
};

}   // namespace legion
