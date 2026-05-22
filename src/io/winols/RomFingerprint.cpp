/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/RomFingerprint.h"

#include <QDataStream>
#include <QIODevice>
#include <QtEndian>
#include <cstring>

#include <algorithm>

namespace winols {

namespace {

// Karp-Rabin rolling-hash parameters.
//   BASE: small prime.  31 is the canonical choice (used in Java's
//         String.hashCode() and many Rabin-Karp implementations).
//   MOD : a prime close to 2^63 to fit hash * BASE in a 128-bit
//         intermediate without overflow when computed via __uint128_t.
constexpr quint64 kBase     = 31ULL;
constexpr quint64 kMod      = (1ULL << 63) - 25;   // 9223372036854775783
constexpr quint64 kEmptySig = 0ULL;                 // sentinel: bucket unused

// 128-bit multiply-then-mod.  mingw13 / GCC provide __uint128_t.
inline quint64 mulmod(quint64 a, quint64 b)
{
    return quint64((__uint128_t(a) * b) % kMod);
}

// SplitMix64 finalizer — full avalanche.  The Karp-Rabin value alone has
// weakly-mixed low bits, and we use the low bits for the bucket; running it
// through SplitMix64 first decorrelates the bucket from the stored value and
// gives each bucket a uniform, high-quality minimum (matters for the very
// structured / repetitive byte patterns in ECU ROMs).
inline quint64 splitmix64(quint64 x)
{
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

inline bool isPadByte(uint8_t b) { return b == 0x00 || b == 0xFF; }

// Walk every byte position of `bytes`, emitting the Karp-Rabin hash of each
// n-byte shingle, finalised through SplitMix64, into the K-bucket MinHash
// sketch (per-bucket minimum, bucket = finalised-hash & (K-1)).
//
// Interior shingles that are a single repeated 0x00 / 0xFF byte (erased
// flash / padding) are SKIPPED — otherwise that one padding hash is the same
// across unrelated ROMs and inflates similarity.  Skip is detected in O(1)
// per byte via a run-length counter.
//
// O(N) total work: one mulmod + one mod-correction per byte.
QList<quint64> oneShingleSketch(const uint8_t *raw, qsizetype N)
{
    QList<quint64> sig(kMinHashK, kEmptySig);
    if (N < kShingleSize) return sig;

    constexpr quint64 mask = kMinHashK - 1;
    static_assert((kMinHashK & (kMinHashK - 1)) == 0,
                  "kMinHashK must be a power of two");

    // Initial shingle: hash of bytes [0, kShingleSize); track the run length
    // of equal bytes ending at each position.
    quint64 h = 0;
    qsizetype run = 1;
    for (int i = 0; i < kShingleSize; ++i) {
        h = (mulmod(h, kBase) + raw[i]) % kMod;
        if (i > 0) run = (raw[i] == raw[i - 1]) ? run + 1 : 1;
    }

    // pow_n = BASE^kShingleSize mod MOD — used to subtract the outgoing
    // byte's contribution as the window slides.
    quint64 powN = 1;
    for (int i = 0; i < kShingleSize; ++i) powN = mulmod(powN, kBase);

    auto offer = [&](quint64 hh) {
        quint64 f = splitmix64(hh);
        if (f == kEmptySig) f = 1;       // never store the sentinel
        const quint64 b = f & mask;
        if (sig[b] == kEmptySig || sig[b] > f) sig[b] = f;
    };
    // shingle [0,kShingleSize): skip if it is one repeated padding byte.
    if (!(run >= kShingleSize && isPadByte(raw[kShingleSize - 1])))
        offer(h);

    // Slide the shingle one byte at a time.
    for (qsizetype i = kShingleSize; i < N; ++i) {
        const quint64 outgoing = mulmod(raw[i - kShingleSize], powN);
        // h = h * BASE - outgoing + raw[i]   (mod MOD); +MOD avoids underflow.
        h = (mulmod(h, kBase) + kMod - outgoing + raw[i]) % kMod;
        run = (raw[i] == raw[i - 1]) ? run + 1 : 1;
        if (run >= kShingleSize && isPadByte(raw[i]))
            continue;                    // uniform 0x00/0xFF shingle — skip
        offer(h);
    }
    return sig;
}

}  // namespace

bool RomFingerprint::isEmpty() const
{
    if (wholeFile.size() != kMinHashK) return true;
    for (quint64 h : wholeFile) if (h != kEmptySig) return false;
    return true;
}

RomFingerprint fingerprint(QByteArrayView romBytes)
{
    RomFingerprint fp;
    if (romBytes.size() < kShingleSize) return fp;

    const auto *raw = reinterpret_cast<const uint8_t *>(romBytes.constData());
    const qsizetype N = romBytes.size();

    // Strip leading / trailing 0x00/0xFF runs (linker padding).
    qsizetype lo = 0;
    while (lo < N && (raw[lo] == 0x00 || raw[lo] == 0xFF)) ++lo;
    qsizetype hi = N;
    while (hi > lo && (raw[hi - 1] == 0x00 || raw[hi - 1] == 0xFF)) --hi;
    fp.bytesScanned = hi - lo;
    if (fp.bytesScanned < kShingleSize) return fp;

    fp.wholeFile = oneShingleSketch(raw + lo, hi - lo);
    // dataArea is reserved & unused by this path — leave it EMPTY so the blob
    // doesn't waste ~1 KB/file storing 128 zeros (halves the on-disk index).
    return fp;
}

SimilarityScore similarity(const RomFingerprint &a, const RomFingerprint &b)
{
    SimilarityScore s;
    auto matchPct = [](const QList<quint64> &x, const QList<quint64> &y,
                       int *matchedOut = nullptr) -> double {
        if (x.size() != kMinHashK || y.size() != kMinHashK) return 0.0;
        int matched = 0, valid = 0;
        for (int i = 0; i < kMinHashK; ++i) {
            if (x[i] == kEmptySig || y[i] == kEmptySig) continue;
            ++valid;
            if (x[i] == y[i]) ++matched;
        }
        if (matchedOut) *matchedOut = matched;
        return valid > 0 ? double(matched) / double(valid) : 0.0;
    };
    int wholeMatched = 0;
    s.wholeFile = matchPct(a.wholeFile, b.wholeFile, &wholeMatched);
    s.dataArea  = matchPct(a.dataArea, b.dataArea);   // reserved (empty → 0)

    // Containment: estimate |A∩B|/min(|A|,|B|) from the Jaccard estimate and
    // the scanned-byte counts (≈ #shingles).  Catches a small ROM fully
    // inside a larger dump, where Jaccard is low purely because of size.
    //   J = |∩|/|∪|,  |∪| = |A|+|B|-|∩|  ⇒  |∩| = J·(|A|+|B|)/(1+J)
    // Gate on ≥4 shared buckets: one shared min-hash is too noisy to claim
    // containment (it would otherwise let a near-padding ROM reach 100%).
    const double J = s.wholeFile;
    if (J > 0.0 && wholeMatched >= 4 && a.bytesScanned > 0 && b.bytesScanned > 0) {
        const double A = double(a.bytesScanned), B = double(b.bytesScanned);
        const double inter = J * (A + B) / (1.0 + J);
        const double mn = std::min(A, B);
        if (mn > 0.0) s.containment = std::min(1.0, inter / mn);
    }

    // Size-mismatch dampener: ROMs differing > 2× in scanned length are
    // usually different ECU families — BUT skip the penalty when one is
    // strongly contained in the other (legit segment-in-dump), and never
    // damp the containment score itself.
    if (a.bytesScanned > 0 && b.bytesScanned > 0) {
        const double ratio = double(std::max(a.bytesScanned, b.bytesScanned))
                           / double(std::min(a.bytesScanned, b.bytesScanned));
        if (ratio > 2.0 && s.containment < 0.6) {
            const double penalty = std::min(0.4, (ratio - 2.0) * 0.15);
            s.wholeFile *= (1.0 - penalty);
            s.dataArea  *= (1.0 - penalty);
        }
    }
    return s;
}

// ─── Serialisation (SQLite BLOB) ────────────────────────────────────────────
// Format (one-perm MinHash, version 3):
//   magic    "RFP3"           4 bytes
//   bytesScanned              8 bytes (little-endian qint64)
//   nWhole                    4 bytes  (== kMinHashK or 0)
//   nData                     4 bytes  (== kMinHashK or 0)
//   wholeFile[]               nWhole * 8 bytes  (per-bucket min, 0 = empty)
//   dataArea[]                nData  * 8 bytes
// Total: 20 + (nWhole + nData) * 8 ≤ 2068 bytes per fingerprint.
//
// RFP1 (unbounded set) and RFP2 (bottom-K sorted) are no longer accepted.
// Old indexes need a one-time `Build similarity index…` rebuild — the
// indexer purges non-RFP3 rows on startup so they get re-fingerprinted.

QByteArray RomFingerprint::toBlob() const
{
    QByteArray out;
    QDataStream s(&out, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s.writeRawData("RFP4", 4);
    s << qint64(bytesScanned)
      << qint32(wholeFile.size())
      << qint32(dataArea.size());          // 0 — dataArea no longer stored
    for (quint64 h : wholeFile) s << h;
    for (quint64 h : dataArea)  s << h;
    return out;
}

RomFingerprint RomFingerprint::fromBlob(const QByteArray &blob)
{
    RomFingerprint fp;
    if (blob.size() < 20) return fp;
    QDataStream s(blob);
    s.setByteOrder(QDataStream::LittleEndian);
    char magic[4] = {0,0,0,0};
    s.readRawData(magic, 4);
    if (std::memcmp(magic, "RFP4", 4) != 0) return fp;
    qint64 scanned;
    qint32 nWhole, nData;
    s >> scanned >> nWhole >> nData;
    if (nWhole < 0 || nWhole > kMinHashK * 4) return fp;
    if (nData  < 0 || nData  > kMinHashK * 4) return fp;
    fp.bytesScanned = scanned;
    fp.wholeFile.reserve(nWhole);
    fp.dataArea.reserve(nData);
    quint64 h;
    for (qint32 i = 0; i < nWhole; ++i) { s >> h; fp.wholeFile.append(h); }
    for (qint32 i = 0; i < nData;  ++i) { s >> h; fp.dataArea.append(h);  }
    return fp;
}

}  // namespace winols
