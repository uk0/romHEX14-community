/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * LEGION public API — the 5-stage pipeline:
 *
 *    detectRegions   → per-voice diff regions (M.1)
 *    inferStructure  → cell-size + dimensions + kind for one region (M.2)
 *    clusterVoices   → group voices by Jaccard of address sets (M.3a)
 *    aggregate       → cross-voice canonical verdicts (M.3)
 *    classify        → tag + rank verdicts (M.4)
 *
 * All five are currently scaffolding (M.0) — they return empty results so
 * the binary links cleanly.  Each gets a real implementation in the
 * corresponding LEGION.* task.
 */

#pragma once

#include "LegionTypes.h"

namespace legion {

// ── M.1 — Per-voice region detection ──────────────────────────────────────
//
// Walks `original` and `stage1` byte-by-byte (must be equal length).  Each
// run of byte differences within `kAdjacency` bytes of each other becomes
// one LegionRegion.  Empty diff returns empty vector.
QVector<LegionRegion>
detectRegions(const QByteArray &original,
              const QByteArray &stage1,
              int               kAdjacency = 16);

// ── M.2 — Structure inference for one region ──────────────────────────────
//
// Tests 1/2/4/8B cell interpretations (byte / 16-bit LoHi+HiLo / 32-bit /
// float / double), picks the one yielding smoothest original sequence.
// Then infers (rows, cols) by autocorrelation between row candidates.
// Classifies kind by cell count.  See task LEGION.2.
struct StructureHint {
    int         cellSize  = 1;
    bool        bigEndian = false;       // true = HiLo, false = LoHi
    int         rows      = 1;
    int         cols      = 1;
    VerdictKind kind      = VerdictKind::Scalar;
};
StructureHint inferStructure(const LegionRegion &region);

// ── M.3a — Voice clustering by intent ─────────────────────────────────────
//
// Groups voices by Jaccard similarity of their addressSets.  Two voices
// whose address sets overlap >= `jaccardMin` land in the same cluster.
// Singletons (clusters with <2 voices) returned at the end of the vector.
// Caller picks one or more clusters to feed into `aggregate`.
QVector<VoiceCluster>
clusterVoices(const QVector<LegionVoice> &voices,
              double                      jaccardMin = 0.50);

// ── M.3 — Aggregate voices in selected cluster ────────────────────────────
//
// For each canonical region (after address-overlap clustering of regions
// across voices), accumulate per-cell meanDelta + stdDevDelta + sample
// count.  Per-verdict local gate: voice's contribution to a given verdict
// is skipped if hamming_similarity(voice.original[range], userBaseline[range])
// < localSimMin (typically 0.90).  Returns unclassified verdicts —
// `classify` does the tagging/ranking afterwards.
QVector<LegionVerdict>
aggregate(const QVector<LegionVoice> &voices,
          const QByteArray           &userBaseline,
          const VoiceCluster         &cluster,
          double                      localSimMin = 0.90);

// ── M.4 — Classification + ranking ────────────────────────────────────────
//
// In-place: tags each verdict (Unanimous / Strong / Majority / Contested /
// Heretic / Checksum / KillRegion) and computes consensusStrength.  Caller
// can then sort by consensusStrength * maxSampleCount and apply a threshold.
void classify(QVector<LegionVerdict> &verdicts,
              int                     totalVoicesInCluster);

// ── M.7 — Apply a verdict to a byte buffer ────────────────────────────────
//
// Reads each cell in @p data, adds round(cell.meanDelta), clamps to the
// unsigned range for cellSize, writes back.  Cells with sampleCount=0 are
// left untouched (no voice contributed).  Out-of-range addresses are
// silently skipped — caller is expected to pass the live user-baseline.
//
// Returns the number of byte positions actually modified — useful for
// status messages and as a sanity check.
int applyVerdict(QByteArray &data, const LegionVerdict &v);

}   // namespace legion
