/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/RomChunkFingerprint.h"
#include "util/Blake3.h"

#include <QByteArrayView>
#include <QHash>
#include <QSet>

#include <cstring>

namespace winols {

namespace {
constexpr uint32_t kBlobMagic   = 0x52434B31;   // 'RCK1'
constexpr uint32_t kBlobVersion = 1;

uint64_t hashChunkU64(QByteArrayView chunk)
{
    const QByteArray h32 = Blake3::hash32(chunk);
    if (h32.size() < 8) return 0;
    uint64_t out = 0;
    const auto *p = reinterpret_cast<const uint8_t *>(h32.constData());
    for (int i = 0; i < 8; ++i) {
        out |= uint64_t(p[i]) << (i * 8);
    }
    return out;
}
} // namespace

RomChunkFingerprint computeChunkFingerprint(QByteArrayView bytes)
{
    RomChunkFingerprint fp;
    fp.fileSize = bytes.size();
    if (bytes.isEmpty()) return fp;

    const qsizetype n = bytes.size();
    fp.chunks.reserve(int((n + RomChunkFingerprint::CHUNK_SIZE - 1)
                          / RomChunkFingerprint::CHUNK_SIZE));
    uint32_t idx = 0;
    for (qsizetype off = 0; off < n; off += RomChunkFingerprint::CHUNK_SIZE) {
        const qsizetype len = qMin<qsizetype>(RomChunkFingerprint::CHUNK_SIZE,
                                              n - off);
        const QByteArrayView slice = bytes.sliced(off, len);
        ChunkHash ch;
        ch.chunkIdx = idx++;
        ch.hash     = hashChunkU64(slice);
        fp.chunks.append(ch);
    }
    return fp;
}

QByteArray RomChunkFingerprint::toBlob() const
{
    QByteArray out;
    out.reserve(4 + 4 + 4 + 8 + 4 + chunks.size() * 12);
    auto put32 = [&out](uint32_t v) {
        char b[4]; for (int i = 0; i < 4; ++i) b[i] = char((v >> (i * 8)) & 0xFF);
        out.append(b, 4);
    };
    auto put64 = [&out](uint64_t v) {
        char b[8]; for (int i = 0; i < 8; ++i) b[i] = char((v >> (i * 8)) & 0xFF);
        out.append(b, 8);
    };
    put32(kBlobMagic);
    put32(kBlobVersion);
    put32(uint32_t(CHUNK_SIZE));
    put64(uint64_t(fileSize));
    put32(uint32_t(chunks.size()));
    for (const auto &c : chunks) {
        put32(c.chunkIdx);
        put64(c.hash);
    }
    return out;
}

RomChunkFingerprint RomChunkFingerprint::fromBlob(QByteArrayView blob)
{
    RomChunkFingerprint fp;
    if (blob.size() < 24) return fp;
    const auto *p = reinterpret_cast<const uint8_t *>(blob.constData());
    qsizetype off = 0;
    auto get32 = [&]() {
        uint32_t v = uint32_t(p[off]) | (uint32_t(p[off+1]) << 8)
                   | (uint32_t(p[off+2]) << 16) | (uint32_t(p[off+3]) << 24);
        off += 4; return v;
    };
    auto get64 = [&]() {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= uint64_t(p[off + i]) << (i * 8);
        off += 8; return v;
    };
    const uint32_t magic   = get32();
    const uint32_t ver     = get32();
    const uint32_t chunkSz = get32();
    const uint64_t fileSz  = get64();
    const uint32_t n       = get32();
    if (magic != kBlobMagic || ver != kBlobVersion
        || chunkSz != uint32_t(CHUNK_SIZE)) return fp;
    if (off + qsizetype(n) * 12 > blob.size()) return fp;
    fp.fileSize = qint64(fileSz);
    fp.chunks.reserve(int(n));
    for (uint32_t i = 0; i < n; ++i) {
        ChunkHash c;
        c.chunkIdx = get32();
        c.hash     = get64();
        fp.chunks.append(c);
    }
    return fp;
}

double chunkContainment(const RomChunkFingerprint &needle,
                        const RomChunkFingerprint &haystack)
{
    if (needle.chunks.isEmpty()) return 0.0;
    QSet<uint64_t> hayHashes;
    hayHashes.reserve(haystack.chunks.size());
    for (const auto &c : haystack.chunks) hayHashes.insert(c.hash);
    int hits = 0;
    for (const auto &c : needle.chunks) {
        if (hayHashes.contains(c.hash)) ++hits;
    }
    return double(hits) / double(needle.chunks.size());
}

}   // namespace winols
