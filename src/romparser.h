/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>

// Result of parsing a ROM file that may be in a text-based format.
struct ParsedROM {
    QByteArray  data;           // flat binary image (padded with 0xFF)
    uint32_t    baseAddress = 0;// lowest physical address found in the file
    bool        ok          = false;
    QString     error;
    QString     format;         // "Binary", "Intel HEX", "Motorola S-record"
};

// Auto-detects format and returns a flat binary image.
// Falls back to raw binary if the format is not recognised.
ParsedROM parseROMFile(const QString &filePath);
ParsedROM parseROMData(const QByteArray &raw);  // detect from in-memory bytes

// ── Writers (Iter 9) ───────────────────────────────────────────────────────
//
// Serialize a flat binary buffer to a hex/S-record text stream.  Caller is
// responsible for picking the format that fits the buffer span (Intel HEX
// uses 32-bit linear addressing via record type 04; S-record handles up to
// 32-bit addresses natively via S3 records).

QByteArray writeIntelHex(const QByteArray &buf, uint32_t baseAddress,
                         int bytesPerRecord = 16);
QByteArray writeSRecord (const QByteArray &buf, uint32_t baseAddress,
                         int bytesPerRecord = 16);
