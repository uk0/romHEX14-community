/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * LEGION — scaffolding (M.0).
 *
 * All five pipeline stages currently return empty results so the binary
 * links cleanly.  Each gets its real implementation in the corresponding
 * LEGION.* iteration.
 */

#include "Legion.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace legion {

namespace {

// Decode `bytes` as a sequence of `cellSize`-byte unsigned ints with given
// endianness.  Returns an empty vector if size doesn't divide evenly.
QVector<double>
decodeCells(const QByteArray &bytes, int cellSize, bool bigEndian)
{
    QVector<double> out;
    if (cellSize <= 0 || cellSize > 8) return out;
    if (bytes.size() % cellSize != 0)  return out;
    const int n = bytes.size() / cellSize;
    const auto *d = reinterpret_cast<const uint8_t *>(bytes.constData());
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const int base = i * cellSize;
        uint64_t v = 0;
        if (bigEndian) {
            for (int b = 0; b < cellSize; ++b) v = (v << 8) | d[base + b];
        } else {
            for (int b = cellSize - 1; b >= 0; --b) v = (v << 8) | d[base + b];
        }
        out.append(double(v));
    }
    return out;
}

// "Roughness" of a cell sequence — mean absolute second derivative
// normalized by mean absolute value.  Lower = smoother (more map-like).
// Returns +infinity for sequences too short or all-zero so they lose the
// "min" comparison in pickCellSize.
double roughness(const QVector<double> &cells)
{
    const int n = cells.size();
    if (n < 3) return std::numeric_limits<double>::infinity();
    double sumAbs2 = 0.0, sumAbs = 0.0;
    for (int i = 1; i < n - 1; ++i) {
        sumAbs2 += std::abs(cells[i + 1] - 2.0 * cells[i] + cells[i - 1]);
    }
    for (int i = 0; i < n; ++i) sumAbs += std::abs(cells[i]);
    if (sumAbs < 1e-9) return std::numeric_limits<double>::infinity();
    return sumAbs2 / sumAbs;
}

struct CellChoice { int size; bool bigEndian; };

// Pick the cell-size + endianness for which the decoded sequence has the
// lowest roughness.  Falls back to sensible defaults when the region is
// too short for the smoothness heuristic (≤ 4 bytes).
CellChoice pickCellSize(const QByteArray &bytes)
{
    const int n = bytes.size();
    if (n <= 1) return {1, false};         // 1 byte → byte
    if (n == 2) return {2, false};         // 16-bit limiter — assume LoHi
    if (n == 3) return {1, false};         // odd size — bytes
    if (n == 4) return {4, false};         // 32-bit limiter — assume LoHi

    struct Cand { int size; bool be; double r; };
    QVector<Cand> cands;
    cands.append({1, false, roughness(decodeCells(bytes, 1, false))});
    if (n % 2 == 0) {
        cands.append({2, false, roughness(decodeCells(bytes, 2, false))});
        cands.append({2, true,  roughness(decodeCells(bytes, 2, true))});
    }
    if (n % 4 == 0) {
        cands.append({4, false, roughness(decodeCells(bytes, 4, false))});
        cands.append({4, true,  roughness(decodeCells(bytes, 4, true))});
    }
    Cand best = cands.first();
    for (const auto &c : cands) if (c.r < best.r) best = c;
    return {best.size, best.be};
}

// Choose (rows, cols) from cell count.  For curves/scalars returns (1, N).
// For maps we test all divisor pairs with aspect ratio in [1:4 .. 4:1] and
// pick the one minimizing total row-to-row absolute difference (i.e. the
// dimensioning where adjacent rows look most alike — typical of real ECU
// maps where the Y-axis is a slowly-varying input like RPM or load).
QPair<int,int> pickDimensions(const QVector<double> &cells)
{
    const int n = cells.size();
    if (n <= 16) return {1, n};

    QVector<QPair<int,int>> candidates;
    for (int rows = 2; rows <= n / 2; ++rows) {
        if (n % rows != 0) continue;
        const int cols = n / rows;
        if (cols < 2) break;
        if (rows > cols * 4 || cols > rows * 4) continue;
        candidates.append({rows, cols});
    }
    if (candidates.isEmpty()) return {1, n};

    double bestScore = std::numeric_limits<double>::infinity();
    QPair<int,int> best = candidates.first();
    for (const auto &c : candidates) {
        const int rows = c.first, cols = c.second;
        double rowDiff = 0.0;
        for (int r = 0; r < rows - 1; ++r) {
            for (int col = 0; col < cols; ++col) {
                rowDiff += std::abs(cells[r * cols + col]
                                    - cells[(r + 1) * cols + col]);
            }
        }
        // Normalize by cells compared so larger dims aren't penalized.
        rowDiff /= double((rows - 1) * cols);
        if (rowDiff < bestScore) { bestScore = rowDiff; best = c; }
    }
    return best;
}

VerdictKind kindByCellCount(int n)
{
    if (n <= 2)   return VerdictKind::Scalar;
    if (n <= 16)  return VerdictKind::Curve;
    if (n <= 256) return VerdictKind::SmallMap;
    return VerdictKind::LargeMap;
}

} // anonymous

// ── M.1 — Per-voice region detection ──────────────────────────────────────
//
// Single linear pass over equal-length buffers.  Each run of byte
// differences whose nearest neighbours are within `kAdjacency` bytes of
// each other becomes one LegionRegion.  When the gap to the next diff
// exceeds kAdjacency, the current region is closed and a new one starts.
//
// Region boundaries are INCLUSIVE first-diff to last-diff — unmodified
// bytes between two diffs inside the same region are kept in the slice
// (sparse-modification context).  Bytes after the last diff are NOT
// included even if within kAdjacency of buffer end.
//
// Buffers of unequal length return empty (caller is expected to align
// first; see projectImportChanges for the offset-search pattern).
QVector<LegionRegion>
detectRegions(const QByteArray &original,
              const QByteArray &stage1,
              int               kAdjacency)
{
    QVector<LegionRegion> regions;
    if (original.size() != stage1.size() || original.isEmpty()) return regions;
    if (kAdjacency < 0) kAdjacency = 0;

    const int   n = original.size();
    const char *o = original.constData();
    const char *s = stage1.constData();

    auto closeRegion = [&](int startIdx, int lastIdx) {
        LegionRegion r;
        r.startAddr     = uint32_t(startIdx);
        r.endAddr       = uint32_t(lastIdx);
        const int len   = lastIdx - startIdx + 1;
        r.originalBytes = original.mid(startIdx, len);
        r.modifiedBytes = stage1  .mid(startIdx, len);
        regions.append(std::move(r));
    };

    int regionStart = -1;
    int lastDiff    = -1;

    for (int i = 0; i < n; ++i) {
        if (o[i] == s[i]) continue;
        if (regionStart < 0) {
            // First diff in a new region.
            regionStart = i;
            lastDiff    = i;
        } else if (i - lastDiff <= kAdjacency) {
            // Gap within tolerance — extend current region.
            lastDiff = i;
        } else {
            // Gap exceeds tolerance — close current, open new.
            closeRegion(regionStart, lastDiff);
            regionStart = i;
            lastDiff    = i;
        }
    }
    if (regionStart >= 0) closeRegion(regionStart, lastDiff);
    return regions;
}

// ── M.2 — Cell-size + dimensions inference ────────────────────────────────
//
// Three-step inference:
//   1) pickCellSize — try 1/2/4-byte cell interpretations + LE/BE, choose
//      the one whose decoded sequence has lowest 2nd-derivative roughness.
//      Short regions (≤ 4 bytes) bypass smoothness and default to the most
//      typical scalar layout (byte / 16-bit LoHi / 32-bit LoHi).
//   2) pickDimensions — for cell counts > 16, walk divisor pairs with
//      aspect ratio in [1:4..4:1] and pick the one minimizing row-to-row
//      absolute difference (rows look alike = real map layout).
//   3) kindByCellCount — Scalar / Curve / SmallMap / LargeMap by N cells.
StructureHint inferStructure(const LegionRegion &region)
{
    StructureHint h;
    const QByteArray &bytes = region.originalBytes;
    if (bytes.isEmpty()) return h;

    const CellChoice cc = pickCellSize(bytes);
    h.cellSize  = cc.size;
    h.bigEndian = cc.bigEndian;

    const QVector<double> cells = decodeCells(bytes, cc.size, cc.bigEndian);
    const auto dims = pickDimensions(cells);
    h.rows = dims.first;
    h.cols = dims.second;
    h.kind = kindByCellCount(cells.size());
    return h;
}

// ── M.3a — Voice clustering by intent ─────────────────────────────────────
//
// Greedy Jaccard clustering on address sets.  Same algorithm validated by
// the legion_probe: sort voices by |addressSet| descending, walk in order,
// each unclaimed voice seeds a new cluster and pulls in every still-unclaimed
// neighbour whose Jaccard with the seed is ≥ jaccardMin.
//
// Why "seed-only" Jaccard (not chained transitive merging)?  Real tuning
// catalogs have noisy outliers — DPF-off files that also poke an unrelated
// limiter byte.  Chaining would merge unrelated intents through the noise.
// Seed-only keeps each cluster anchored to its biggest member's footprint.
//
// Output is sorted by cluster size descending; the caller (UI / Lua) picks
// which clusters to feed into aggregate().  Singletons are included so the
// caller can decide whether to surface them.
namespace {

int jaccardIntersect(const QSet<uint32_t> &a, const QSet<uint32_t> &b)
{
    // Iterate the smaller set against the larger — QSet lookup is O(1) avg.
    const QSet<uint32_t> &small = a.size() <= b.size() ? a : b;
    const QSet<uint32_t> &large = a.size() <= b.size() ? b : a;
    int inter = 0;
    for (auto it = small.constBegin(); it != small.constEnd(); ++it) {
        if (large.contains(*it)) ++inter;
    }
    return inter;
}

double jaccard(const QSet<uint32_t> &a, const QSet<uint32_t> &b)
{
    const int inter = jaccardIntersect(a, b);
    const int uni   = a.size() + b.size() - inter;
    if (uni <= 0) return 0.0;
    return double(inter) / double(uni);
}

// Filename keyword scan — same word list as the probe.
void scanKeywords(const QString &path, QHash<QString,int> &hits)
{
    const QString lower = path.toLower();
    static const QString kw[] = {
        QStringLiteral("stage"),
        QStringLiteral("dpf"),
        QStringLiteral("egr"),
        QStringLiteral("adblue"),
        QStringLiteral("immo"),
        QStringLiteral("lambda"),
        QStringLiteral("dtc"),
    };
    for (const QString &k : kw) {
        if (lower.contains(k)) hits[k] = hits.value(k, 0) + 1;
    }
}

QString labelFromKeywords(const QHash<QString,int> &kw, int members)
{
    if (kw.isEmpty()) return QStringLiteral("unlabeled");
    QString bestKey;
    int bestCount = 0;
    for (auto it = kw.constBegin(); it != kw.constEnd(); ++it) {
        if (it.value() > bestCount) { bestCount = it.value(); bestKey = it.key(); }
    }
    if (bestKey.isEmpty()) return QStringLiteral("unlabeled");
    // Dominance threshold: >=50% of cluster filenames mention the keyword.
    if (bestCount * 2 >= members) return bestKey + QStringLiteral("-like");
    return QStringLiteral("mixed (") + bestKey + QStringLiteral(")");
}

} // anonymous

QVector<VoiceCluster>
clusterVoices(const QVector<LegionVoice> &voices,
              double                      jaccardMin)
{
    QVector<VoiceCluster> clusters;
    const int n = voices.size();
    if (n == 0) return clusters;

    // Order voices by addressSet size descending — biggest seeds first.
    QVector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return voices[a].addressSet.size() > voices[b].addressSet.size();
    });

    QVector<bool> claimed(n, false);

    for (int idx : order) {
        if (claimed[idx]) continue;
        claimed[idx] = true;

        VoiceCluster c;
        c.voiceIndices.append(idx);

        const QSet<uint32_t> &seed = voices[idx].addressSet;
        for (int j : order) {
            if (j == idx || claimed[j]) continue;
            if (voices[j].addressSet.isEmpty()) continue;
            if (jaccard(seed, voices[j].addressSet) >= jaccardMin) {
                c.voiceIndices.append(j);
                claimed[j] = true;
            }
        }

        // Consensus addresses: touched by >= 50% of cluster members.
        QHash<uint32_t,int> hitCount;
        for (int m : c.voiceIndices) {
            const auto &set = voices[m].addressSet;
            for (auto it = set.constBegin(); it != set.constEnd(); ++it) {
                hitCount[*it] = hitCount.value(*it, 0) + 1;
            }
        }
        const int halfMembers = (c.voiceIndices.size() + 1) / 2;
        uint32_t minA = std::numeric_limits<uint32_t>::max();
        uint32_t maxA = 0;
        int consensus = 0;
        for (auto it = hitCount.constBegin(); it != hitCount.constEnd(); ++it) {
            if (it.value() >= halfMembers) {
                ++consensus;
                if (it.key() < minA) minA = it.key();
                if (it.key() > maxA) maxA = it.key();
            }
        }
        c.addrRangeMin       = consensus > 0 ? minA : 0;
        c.addrRangeMax       = consensus > 0 ? maxA : 0;
        c.consensusAddrCount = consensus;

        // Filename keyword scan.
        for (int m : c.voiceIndices) scanKeywords(voices[m].sourcePath, c.filenameKeywords);
        c.label = labelFromKeywords(c.filenameKeywords, c.voiceIndices.size());

        clusters.append(std::move(c));
    }

    // Largest cluster first — caller usually wants the strongest signal.
    std::sort(clusters.begin(), clusters.end(),
              [](const VoiceCluster &a, const VoiceCluster &b) {
        return a.voiceIndices.size() > b.voiceIndices.size();
    });
    return clusters;
}

// ── M.3 — Cross-voice aggregation ─────────────────────────────────────────
//
// Pipeline within one selected cluster:
//   1) Collect every changed address from cluster members → one set.
//   2) Run-length cluster those addresses into canonical regions
//      (same adjacency K=16 as detectRegions).
//   3) For each canonical region:
//      a) inferStructure on userBaseline bytes in the range (this is the
//         "shape of the slot" the verdict will occupy in the user's ROM).
//      b) Apply DUAL-TIER similarity gate per voice:
//           - global tier was already enforced when caller selected the
//             cluster (every voice has overall-similarity ≥ 85%).
//           - local tier: fraction of voice's *touched* addresses in this
//             range where voice.original byte matches userBaseline ≥
//             localSimMin (default 0.90).  Skips voices whose source ROM
//             diverged from user's in this specific slot.
//      c) For each cell (cellSize-byte chunk), each gated-in voice that
//         covers the full cell contributes delta = decode(modified) -
//         decode(original).  Accumulate mean / stdDev / sampleCount.
//   4) Return verdicts with structure filled in but tag left default —
//      classify() handles tagging & ranking afterwards.
namespace {

// Look up the byte voice gave at `addr`, side=0 → original, side=1 →
// modified.  Returns false if voice doesn't cover that address.  Regions
// inside voice are sorted by startAddr (detectRegions emits in order).
bool voiceByteAt(const LegionVoice &v, uint32_t addr, int side, uint8_t *out)
{
    int lo = 0, hi = v.regions.size() - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const auto &r = v.regions[mid];
        if (addr < r.startAddr) { hi = mid - 1; continue; }
        if (addr > r.endAddr)   { lo = mid + 1; continue; }
        const QByteArray &buf = side == 0 ? r.originalBytes : r.modifiedBytes;
        const int off = int(addr - r.startAddr);
        if (off < 0 || off >= buf.size()) return false;
        *out = uint8_t(buf[off]);
        return true;
    }
    return false;
}

uint64_t decodeUnsigned(const uint8_t *p, int cellSize, bool bigEndian)
{
    uint64_t v = 0;
    if (bigEndian) {
        for (int b = 0; b < cellSize; ++b) v = (v << 8) | p[b];
    } else {
        for (int b = cellSize - 1; b >= 0; --b) v = (v << 8) | p[b];
    }
    return v;
}

} // anonymous

QVector<LegionVerdict>
aggregate(const QVector<LegionVoice> &voices,
          const QByteArray           &userBaseline,
          const VoiceCluster         &cluster,
          double                      localSimMin)
{
    QVector<LegionVerdict> out;
    if (cluster.voiceIndices.isEmpty() || userBaseline.isEmpty()) return out;

    constexpr int kAdj = 16;

    // 1) Union of every touched address from cluster voices.
    QSet<uint32_t> all;
    for (int idx : cluster.voiceIndices) {
        if (idx < 0 || idx >= voices.size()) continue;
        const auto &set = voices[idx].addressSet;
        for (auto it = set.constBegin(); it != set.constEnd(); ++it) {
            all.insert(*it);
        }
    }
    if (all.isEmpty()) return out;

    // 2) Sort + run-length cluster by adjacency K.
    QVector<uint32_t> sorted(all.begin(), all.end());
    std::sort(sorted.begin(), sorted.end());

    struct Range { uint32_t s, e; };
    QVector<Range> ranges;
    uint32_t curS = sorted.first(), curE = sorted.first();
    for (int i = 1; i < sorted.size(); ++i) {
        if (sorted[i] - curE <= uint32_t(kAdj)) {
            curE = sorted[i];
        } else {
            ranges.append({curS, curE});
            curS = sorted[i];
            curE = sorted[i];
        }
    }
    ranges.append({curS, curE});

    const uint32_t baseSize = uint32_t(userBaseline.size());

    // 3) Build one verdict per canonical range.
    for (const auto &range : ranges) {
        if (range.e >= baseSize) continue;     // outside user's ROM
        const int rangeLen = int(range.e - range.s + 1);

        // 3a) Structure inference from user's baseline slice.
        LegionRegion baseSlice;
        baseSlice.startAddr     = range.s;
        baseSlice.endAddr       = range.e;
        baseSlice.originalBytes = userBaseline.mid(int(range.s), rangeLen);
        baseSlice.modifiedBytes = baseSlice.originalBytes;
        const StructureHint h = inferStructure(baseSlice);

        const int cellSize = qBound(1, h.cellSize, 8);
        const int nCells   = rangeLen / cellSize;
        if (nCells < 1) continue;

        LegionVerdict v;
        v.startAddr = range.s;
        v.endAddr   = range.s + uint32_t(nCells * cellSize - 1);
        v.cellSize  = cellSize;
        v.bigEndian = h.bigEndian;
        v.rows      = h.rows;
        v.cols      = h.cols;
        v.kind      = h.kind;
        v.cells.resize(nCells);

        QVector<double> sumD(nCells, 0.0);
        QVector<double> sumSqD(nCells, 0.0);
        QVector<int>    cnt(nCells, 0);

        // 3b/c) Per-voice gating + per-cell delta accumulation.
        for (int vi : cluster.voiceIndices) {
            if (vi < 0 || vi >= voices.size()) continue;
            const auto &voice = voices[vi];

            // Local hamming: fraction of voice's touched-and-in-range bytes
            // where voice.original equals userBaseline at that address.
            int touched = 0, matching = 0;
            for (uint32_t a = range.s; a <= range.e; ++a) {
                uint8_t b;
                if (!voiceByteAt(voice, a, /*side=*/0, &b)) continue;
                ++touched;
                if (uint8_t(userBaseline[int(a)]) == b) ++matching;
            }
            if (touched == 0) continue;
            const double localSim = double(matching) / double(touched);
            if (localSim < localSimMin) continue;

            v.contributingVoices.append(vi);

            // Per-cell contribution: voice must cover ALL cellSize bytes.
            uint8_t origBuf[8] = {0}, modBuf[8] = {0};
            for (int ci = 0; ci < nCells; ++ci) {
                const uint32_t cs = range.s + uint32_t(ci * cellSize);
                bool full = true;
                for (int b = 0; b < cellSize; ++b) {
                    if (!voiceByteAt(voice, cs + uint32_t(b), 0, &origBuf[b]) ||
                        !voiceByteAt(voice, cs + uint32_t(b), 1, &modBuf [b])) {
                        full = false; break;
                    }
                }
                if (!full) continue;
                const int64_t oV = int64_t(decodeUnsigned(origBuf, cellSize, h.bigEndian));
                const int64_t mV = int64_t(decodeUnsigned(modBuf,  cellSize, h.bigEndian));
                const double  d  = double(mV - oV);
                sumD[ci]   += d;
                sumSqD[ci] += d * d;
                ++cnt[ci];
            }
        }

        // 3c) Finalize per-cell statistics.
        int maxCount = 0;
        for (int ci = 0; ci < nCells; ++ci) {
            const int c = cnt[ci];
            v.cells[ci].sampleCount = c;
            if (c > 0) {
                const double mean   = sumD  [ci] / double(c);
                const double meanSq = sumSqD[ci] / double(c);
                v.cells[ci].meanDelta   = mean;
                v.cells[ci].stdDevDelta = std::sqrt(std::max(0.0, meanSq - mean * mean));
            }
            if (c > maxCount) maxCount = c;
        }
        v.maxSampleCount = maxCount;
        if (maxCount > 0) out.append(std::move(v));
    }
    return out;
}

// ── M.4 — Classification + ranking signal ─────────────────────────────────
//
// In-place tagging of each verdict + per-verdict consensusStrength (0..1)
// used by the UI to sort and threshold.  Tag logic:
//
//   Heretic         — densest cell sampled by <10% of cluster voices.
//                     Filter-out-by-default noise.
//   Unanimous       — every cluster voice contributed AND voices agree
//                     tightly on magnitude (CV = stdDev/|mean| < 0.05).
//   StrongConsensus — ≥80% of cluster contributed AND CV < 0.20.
//   Majority        — ≥50% of cluster contributed (any agreement).
//   Contested       — between Heretic and Majority thresholds — voices
//                     touched the slot but disagree.
//
// consensusStrength = coverage × agreement, where
//   coverage  = densestSampleCount / totalVoicesInCluster
//   agreement = clamp(1 - min(CV, 1), 0..1)
// → 0.0 means "no consensus", 1.0 means "every voice, all agreed".
//
// Checksum and KillRegion tags from the enum are not detected here — they
// require external context (segment layout, original byte values) and are
// left to a future iteration that has access to those signals.
void classify(QVector<LegionVerdict> &verdicts,
              int                     totalVoicesInCluster)
{
    if (totalVoicesInCluster <= 0) return;

    auto ceilFrac = [](int total, int num, int den) {
        return (total * num + den - 1) / den;
    };
    const int thrUnanimous = totalVoicesInCluster;
    const int thrStrong    = ceilFrac(totalVoicesInCluster, 80, 100);
    const int thrMajority  = ceilFrac(totalVoicesInCluster, 50, 100);
    const int thrHeretic   = ceilFrac(totalVoicesInCluster, 10, 100);

    for (auto &v : verdicts) {
        // Densest cell — picks the contribution-count peak of the verdict.
        int dIdx = -1;
        for (int i = 0; i < v.cells.size(); ++i) {
            if (dIdx < 0 || v.cells[i].sampleCount > v.cells[dIdx].sampleCount)
                dIdx = i;
        }
        if (dIdx < 0 || v.cells[dIdx].sampleCount == 0) {
            v.tag = VerdictTag::Heretic;
            v.consensusStrength = 0.0;
            continue;
        }

        const CellStats &c = v.cells[dIdx];
        const int    sc   = c.sampleCount;
        const double mean = c.meanDelta;
        const double sd   = c.stdDevDelta;

        // Coefficient of variation — agreement among voices on magnitude.
        // Sentinel for mean ≈ 0: if sd is also tiny → cv 0 (degenerate
        // "no change"), otherwise cv huge (voices disagree wildly).
        double cv;
        if (std::abs(mean) < 1e-9) cv = (sd < 1e-9) ? 0.0 : 1.0e9;
        else                       cv = sd / std::abs(mean);

        if (sc < thrHeretic) {
            v.tag = VerdictTag::Heretic;
        } else if (sc >= thrUnanimous && cv < 0.05) {
            v.tag = VerdictTag::Unanimous;
        } else if (sc >= thrStrong && cv < 0.20) {
            v.tag = VerdictTag::StrongConsensus;
        } else if (sc >= thrMajority) {
            v.tag = VerdictTag::Majority;
        } else {
            v.tag = VerdictTag::Contested;
        }

        const double coverage  = double(sc) / double(totalVoicesInCluster);
        const double agreement = std::max(0.0, 1.0 - std::min(cv, 1.0));
        v.consensusStrength    = coverage * agreement;
    }

    // Rank: strongest consensus first.
    std::sort(verdicts.begin(), verdicts.end(),
              [](const LegionVerdict &a, const LegionVerdict &b) {
        return a.consensusStrength > b.consensusStrength;
    });
}

// ── M.7 — apply a verdict to a byte buffer ────────────────────────────────
int applyVerdict(QByteArray &data, const LegionVerdict &v)
{
    if (v.cellSize < 1 || v.cellSize > 8) return 0;
    if (v.cells.isEmpty())                return 0;
    if (data.isEmpty())                   return 0;

    const uint64_t mask = (v.cellSize == 8)
        ? ~uint64_t(0)
        : (uint64_t(1) << (v.cellSize * 8)) - 1;

    int changed = 0;
    for (int ci = 0; ci < v.cells.size(); ++ci) {
        const auto &c = v.cells[ci];
        if (c.sampleCount <= 0) continue;
        const int64_t rounded = int64_t(std::llround(c.meanDelta));
        if (rounded == 0) continue;

        const uint32_t cs = v.startAddr + uint32_t(ci * v.cellSize);
        if (cs + uint32_t(v.cellSize) > uint32_t(data.size())) break;

        // Read current.
        uint64_t cur = 0;
        if (v.bigEndian) {
            for (int b = 0; b < v.cellSize; ++b)
                cur = (cur << 8) | uint8_t(data[int(cs) + b]);
        } else {
            for (int b = v.cellSize - 1; b >= 0; --b)
                cur = (cur << 8) | uint8_t(data[int(cs) + b]);
        }

        // Apply delta with unsigned-clamp semantics.
        int64_t next64 = int64_t(cur) + rounded;
        if (next64 < 0)               next64 = 0;
        else if (uint64_t(next64) > mask) next64 = int64_t(mask);
        const uint64_t next = uint64_t(next64);

        if (next == cur) continue;

        // Write back.
        if (v.bigEndian) {
            for (int b = v.cellSize - 1; b >= 0; --b) {
                uint8_t byte = uint8_t((next >> ((v.cellSize - 1 - b) * 8)) & 0xFF);
                if (uint8_t(data[int(cs) + b]) != byte) ++changed;
                data[int(cs) + b] = char(byte);
            }
        } else {
            for (int b = 0; b < v.cellSize; ++b) {
                uint8_t byte = uint8_t((next >> (b * 8)) & 0xFF);
                if (uint8_t(data[int(cs) + b]) != byte) ++changed;
                data[int(cs) + b] = char(byte);
            }
        }
    }
    return changed;
}

}   // namespace legion
