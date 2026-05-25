/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "KpImporter.h"
#include "OlsKennfeldParser.h"
#include "ZipDecompressor.h"

#include <QtEndian>
#include <QFileInfo>
#include <cstring>

namespace ols {


bool KpImporter::extractInternEntry(const QByteArray &fileData,
                                     QByteArray &compressed,
                                     uint32_t &uncompressedSize,
                                     uint16_t &method,
                                     QString &err)
{
    static const char pkSig[] = { 'P', 'K', '\x03', '\x04' };
    int lfhOff = -1;
    for (qsizetype i = 0; i + 4 <= fileData.size(); ++i) {
        if (std::memcmp(fileData.constData() + i, pkSig, 4) == 0) {
            lfhOff = static_cast<int>(i);
            break;
        }
    }
    if (lfhOff < 0) {
        err = KpImporter::tr("No PKZIP local file header (PK\\x03\\x04) found");
        return false;
    }

    if (lfhOff + 30 > fileData.size()) {
        err = KpImporter::tr("Truncated ZIP local file header");
        return false;
    }

    const auto *h = reinterpret_cast<const uchar *>(
        fileData.constData() + lfhOff);

    method = qFromLittleEndian<uint16_t>(h + 8);
    uint32_t csize = qFromLittleEndian<uint32_t>(h + 18);
    uncompressedSize = qFromLittleEndian<uint32_t>(h + 22);
    uint16_t fnLen = qFromLittleEndian<uint16_t>(h + 26);
    uint16_t extraLen = qFromLittleEndian<uint16_t>(h + 28);

    qsizetype fnStart = lfhOff + 30;
    if (fnStart + fnLen > fileData.size()) {
        err = KpImporter::tr("Truncated ZIP filename");
        return false;
    }
    QString filename = QString::fromLatin1(
        fileData.constData() + fnStart, fnLen);
    if (filename != QStringLiteral("intern")) {
    }

    qsizetype dataStart = fnStart + fnLen + extraLen;
    if (dataStart + static_cast<qsizetype>(csize) > fileData.size()) {
        err = KpImporter::tr("ZIP compressed data extends beyond file end "
                             "(offset 0x%1, size %2, file %3)")
                  .arg(dataStart, 0, 16)
                  .arg(csize)
                  .arg(fileData.size());
        return false;
    }

    compressed = fileData.mid(static_cast<int>(dataStart),
                               static_cast<int>(csize));
    return true;
}


KpImportResult KpImporter::importFromBytes(const QByteArray &fileData,
                                            uint32_t baseAddress)
{
    KpImportResult result;

    if (fileData.size() < 24) {
        result.error = KpImporter::tr("File too small for KP header (%1 bytes)")
                           .arg(fileData.size());
        return result;
    }

    static const char magic[] = "WinOLS File";
    uint32_t magicLen = qFromLittleEndian<uint32_t>(
        reinterpret_cast<const uchar *>(fileData.constData()));
    if (magicLen != 11
        || std::memcmp(fileData.constData() + 4, magic, 11) != 0) {
        result.error = KpImporter::tr("Invalid OLS magic header");
        return result;
    }

    result.formatVersion = qFromLittleEndian<uint32_t>(
        reinterpret_cast<const uchar *>(fileData.constData() + 16));
    result.declaredFileSize = qFromLittleEndian<uint32_t>(
        reinterpret_cast<const uchar *>(fileData.constData() + 20));

    if (result.declaredFileSize != static_cast<uint32_t>(fileData.size())) {
        result.warnings.append(
            KpImporter::tr("Header declares file size %1 but actual is %2")
                .arg(result.declaredFileSize)
                .arg(fileData.size()));
    }

    QByteArray compressed;
    uint32_t uncompressedSize = 0;
    uint16_t method = 0;
    QString extractErr;

    if (!extractInternEntry(fileData, compressed, uncompressedSize,
                            method, extractErr)) {
        result.error = extractErr;
        return result;
    }

    QByteArray intern;
    if (method == 8) {
        QString inflateErr;
        intern = ZipDecompressor::decompress(compressed,
                                             static_cast<qsizetype>(uncompressedSize),
                                             &inflateErr);
        if (intern.isEmpty()) {
            result.error = KpImporter::tr("Failed to inflate intern: %1")
                               .arg(inflateErr);
            return result;
        }
    } else if (method == 0) {
        intern = compressed;
    } else {
        result.error = KpImporter::tr("Unsupported ZIP compression method %1")
                           .arg(method);
        return result;
    }

    if (static_cast<uint32_t>(intern.size()) != uncompressedSize) {
        result.warnings.append(
            KpImporter::tr("Decompressed size %1 != declared %2")
                .arg(intern.size())
                .arg(uncompressedSize));
    }

    // Route the intern payload through OlsKennfeldParser — the same
    // schema-aware parser used for .ols imports.  RE'd via WinOLS's
    // sub_7FF72E82DEA0 (TaggedName serialiser, IDA 0x7FF72E82DEA0):
    // .kp intern records share the .ols intern record layout except that
    // the identifier-name cstring is empty (the human-readable name lives
    // in the earlier comment field).  parseOne tolerates that case — see
    // its matching change in OlsKennfeldParser::parseOne (issue #18).
    const int schema = static_cast<int>(result.formatVersion);
    result.maps = OlsKennfeldParser::parseIntern(intern, schema, &result.warnings);
    result.mapCount = static_cast<uint32_t>(result.maps.size());

    // Rebase ROM addresses to project file offsets if requested. parseIntern
    // stores rawAddress as it appears in the file; callers with a project
    // baseAddress get the file offset in m.address.
    if (baseAddress != 0) {
        for (auto &m : result.maps) {
            m.address = (m.rawAddress >= baseAddress)
                            ? m.rawAddress - baseAddress
                            : m.rawAddress;
        }
    }

    return result;
}

}
