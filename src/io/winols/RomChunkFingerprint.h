/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Chunk-level content fingerprint for true byte-twin discovery.
 *
 * The existing RomFingerprint (MinHash over rolling n-grams) is great for
 * "files share many code patterns" — it matches ECUs of the same family
 * regardless of calibration.  That is the WRONG metric when the user
 * wants "files with mostly the same bytes" (e.g., tuning twins of one
 * specific car).  Two EDC17C46 ROMs of different vehicles share ~98% of
 * MinHash shingles but only ~13% of actual bytes.
 *
 * This fingerprint slices each ROM into fixed CHUNK_SIZE blocks (16 KB),
 * hashes each with BLAKE3 truncated to 64 bits, and stores the resulting
 * (chunk_index, hash) pairs.  Similarity becomes simple set-containment:
 *
 *   containment(needle, haystack) =
 *       |hashes(needle) ∩ hashes(haystack)| / |hashes(needle)|
 *
 * A small tuning edit ruins at most a handful of chunks; everything else
 * stays identical, so byte-twins land near 1.0 while different ECUs
 * land near 0.
 *
 * Per file: 16 KB chunks × 8-byte hash = 0.5% storage overhead.
 * For a 2 MB ROM that is 128 chunks × 8 B = 1 KB.
 */

#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QVector>
#include <cstdint>

namespace winols {

struct ChunkHash {
    uint32_t chunkIdx = 0;     // index into the file: byteOffset = chunkIdx * CHUNK_SIZE
    uint64_t hash    = 0;      // BLAKE3 truncated to the first 8 bytes
};

struct RomChunkFingerprint {
    static constexpr int CHUNK_SIZE = 16 * 1024;   // 16 KB
    QVector<ChunkHash> chunks;
    qint64             fileSize = 0;

    bool isEmpty() const { return chunks.isEmpty(); }
    int  chunkCount() const { return chunks.size(); }

    /// Pack to a portable byte blob for SQLite storage.  Layout:
    ///   uint32 version (1)
    ///   uint32 chunkSize
    ///   int64  fileSize
    ///   uint32 nChunks
    ///   nChunks × (uint32 idx + uint64 hash)
    /// All little-endian.
    QByteArray toBlob() const;

    /// Parse a blob produced by toBlob().  Returns empty fingerprint
    /// on any error (corrupt or wrong-version blob).
    static RomChunkFingerprint fromBlob(QByteArrayView blob);
};

/// Compute chunk fingerprint over @p bytes.  Last partial chunk (if any)
/// is hashed as-is, NOT zero-padded — keeps fingerprint stable when the
/// same file is truncated/padded by other tools.  Empty input returns
/// an empty fingerprint.
RomChunkFingerprint computeChunkFingerprint(QByteArrayView bytes);

/// Set-containment: share of the needle's chunk hashes that also appear
/// in the haystack.  Range 0..1.  1.0 means the haystack contains every
/// chunk of the needle (or both files are byte-identical).
double chunkContainment(const RomChunkFingerprint &needle,
                        const RomChunkFingerprint &haystack);

}   // namespace winols
