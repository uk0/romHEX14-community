/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "OlsImporter.h"
#include "CArchiveReader.h"
#include "OlsHeader.h"
#include "OlsMagicScanner.h"
#include "OlsProjectMetadata.h"
#include "OlsVersionDirectory.h"
#include "OlsRomExtractor.h"
#include "OlsKennfeldParser.h"

#include <QHash>
#include <QtEndian>
#include <cstring>

namespace ols {

OlsImportResult OlsImporter::importFromBytes(const QByteArray &fileData)
{
    OlsImportResult result;

    result.header = OlsHeader::parse(fileData);
    if (!result.header.valid) {
        result.error = result.header.error;
        return result;
    }

    uint32_t fmtVer = result.header.formatVersion;

    if (fmtVer >= 200
        && result.header.declaredFileSize != static_cast<uint32_t>(fileData.size())) {
        result.warnings.append(
            OlsImporter::tr("Header declares size %1 but actual is %2")
                .arg(result.header.declaredFileSize)
                .arg(fileData.size()));
    }

    int schema = static_cast<int>(fmtVer);

    MagicAnchors anchors = OlsMagicScanner::scan(fileData, 1008);

    qsizetype metadataStart = (fmtVer >= 200) ? 0x18 : 0x14;
    try {
        CArchiveReader reader(fileData, metadataStart, fmtVer);
        result.metadata = OlsProjectMetadata::parse(reader, &result.warnings);
    } catch (const std::exception &e) {
        result.warnings.append(
            OlsImporter::tr("Project metadata parse failed: %1")
                .arg(QString::fromStdString(e.what())));
    }

    OlsVersionDirectory versionDir = OlsVersionDirectory::parse(
        fileData, &result.warnings);

    QVector<OlsRomResult> romVersions = OlsRomExtractor::extractAll(fileData);

    auto [mapRegionStart, mapRegionEnd] =
        OlsKennfeldParser::findMapRegion(fileData, schema);

    QVector<MapInfo> maps = OlsKennfeldParser::parseAll(
        fileData, mapRegionStart, mapRegionEnd, schema, &result.warnings);


    QStringList versionNames;
    {
        const QString &tag = result.metadata.revisionTag;
        int po = tag.indexOf(QLatin1Char('('));
        int pc = tag.lastIndexOf(QLatin1Char(')'));
        if (po >= 0 && pc > po) {
            QString inner = tag.mid(po + 1, pc - po - 1);
            QStringList entries = inner.split(QStringLiteral(",\t"), Qt::SkipEmptyParts);
            if (entries.isEmpty())
                entries = inner.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const QString &e : entries) {
                QString s = e.trimmed();
                int colon = s.indexOf(QStringLiteral(":\t"));
                if (colon < 0) colon = s.indexOf(QLatin1Char(':'));
                if (colon > 0) s = s.left(colon).trimmed();
                if (!s.isEmpty()) versionNames.append(s);
            }
        }
    }

    if (!romVersions.isEmpty() && !romVersions[0].error.isEmpty()
        && romVersions.size() == 1) {
        result.warnings.append(romVersions[0].error);
        OlsVersion ver;
        ver.name = versionNames.value(0, OlsImporter::tr("Default"));
        ver.maps = maps;
        result.versions.append(std::move(ver));
    } else {
        // When map records share a uniform virtual base address, treat the
        // file tail of length universalBase as the assembled ROM so rom_addr
        // indexes it directly.  Gated on formatVersion >= 440 where the
        // legacy segment layout no longer aligns with the stored rom_addr.
        uint32_t sharedUniversalBase = 0;
        bool universalBaseConsistent = false;
        if (fmtVer >= 440) {
            QHash<uint32_t, int> counts;
            int considered = 0;
            for (const auto &m : maps) {
                if (m.olsUniversalBase != 0 && m.olsUniversalBase != 0xFFFFFFFF
                    && m.rawAddress != 0 && m.rawAddress != 0xFFFFFFFF
                    && m.rawAddress < m.olsUniversalBase) {
                    counts[m.olsUniversalBase]++;
                    ++considered;
                }
            }
            int bestCount = 0;
            for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
                if (it.value() > bestCount) {
                    bestCount = it.value();
                    sharedUniversalBase = it.key();
                }
            }
            // Apply only when overwhelmingly consistent (>=90% of qualifying maps),
            // the file actually contains a tail of length sharedUniversalBase, and
            // the rom_addr range of those maps falls inside the tail.
            if (considered >= 4 && bestCount * 10 >= considered * 9
                && static_cast<qsizetype>(sharedUniversalBase) > 0x1000
                && static_cast<qsizetype>(sharedUniversalBase) <= fileData.size()) {
                universalBaseConsistent = true;
            }
        }

        for (auto &rv : romVersions) {
            OlsVersion ver;
            ver.name = versionNames.value(rv.versionIndex,
                           OlsImporter::tr("Version %1").arg(rv.versionIndex));
            ver.romData = rv.assembledRom;
            ver.segments = rv.segments;
            ver.byteOrder = ByteOrder::LittleEndian;

            uint32_t baseAddr = 0;
            if (!rv.segments.isEmpty()) {
                baseAddr = rv.segments[0].flashBase + rv.segments[0].framingBytes;
                for (const auto &seg : rv.segments) {
                    const uint32_t eff = seg.flashBase + seg.framingBytes;
                    if (eff < baseAddr)
                        baseAddr = eff;
                }
            }

            // Universal-base override: replace assembled ROM with the file tail
            // of length a644, so rom_addr (a624) indexes it directly.  Only fires
            // when the per-record a644 is uniformly set across kennfeld records.
            if (universalBaseConsistent && rv.versionIndex == 0) {
                const qsizetype tailLen = sharedUniversalBase;
                if (tailLen > 0 && tailLen <= fileData.size()) {
                    ver.romData = fileData.right(tailLen);
                    baseAddr = 0;
                }
            }

            QVector<MapInfo> translatedMaps = maps;
            for (auto &m : translatedMaps) {
                m.rawAddress = m.address;
                m.address = (m.rawAddress >= baseAddr)
                    ? m.rawAddress - baseAddr : m.rawAddress;
                if (m.xAxis.hasPtsAddress && m.xAxis.ptsAddress >= baseAddr)
                    m.xAxis.ptsAddress -= baseAddr;
                if (m.yAxis.hasPtsAddress && m.yAxis.ptsAddress >= baseAddr)
                    m.yAxis.ptsAddress -= baseAddr;
            }
            for (auto &m : translatedMaps) {
                auto readAxisBreakpoints = [&](AxisInfo &ax, int count) {
                    if (!ax.hasPtsAddress || ax.ptsAddress == 0
                        || ax.ptsAddress == 0xFFFFFFFF || count <= 0)
                        return;

                    int cellBytes = ax.ptsDataSize;
                    if (cellBytes <= 0)
                        cellBytes = (m.dataSize > 0) ? m.dataSize : 2;
                    if (cellBytes != 1 && cellBytes != 2 && cellBytes != 4
                        && cellBytes != 8)
                        cellBytes = 2;

                    int startOff = static_cast<int>(ax.ptsAddress);
                    if (startOff < 0
                        || startOff + count * cellBytes > ver.romData.size())
                        return;
                    const auto *rom = reinterpret_cast<const uint8_t *>(
                        ver.romData.constData());

                    const uint32_t dt = ax.ptsDataType;
                    const bool sgn = ax.ptsSigned;
                    auto decodeOne = [&](int off) -> double {
                        switch (dt) {
                        case 2: {
                            uint16_t v = (uint16_t(rom[off]) << 8) | rom[off + 1];
                            return sgn ? double(int16_t(v)) : double(v);
                        }
                        case 3: {
                            uint16_t v = uint16_t(rom[off])
                                       | (uint16_t(rom[off + 1]) << 8);
                            return sgn ? double(int16_t(v)) : double(v);
                        }
                        case 4: {
                            uint32_t v = (uint32_t(rom[off]) << 24)
                                       | (uint32_t(rom[off + 1]) << 16)
                                       | (uint32_t(rom[off + 2]) << 8)
                                       |  uint32_t(rom[off + 3]);
                            return sgn ? double(int32_t(v)) : double(v);
                        }
                        case 5: {
                            uint32_t v = uint32_t(rom[off])
                                       | (uint32_t(rom[off + 1]) << 8)
                                       | (uint32_t(rom[off + 2]) << 16)
                                       | (uint32_t(rom[off + 3]) << 24);
                            return sgn ? double(int32_t(v)) : double(v);
                        }
                        case 6: {
                            uint32_t v = (uint32_t(rom[off]) << 24)
                                       | (uint32_t(rom[off + 1]) << 16)
                                       | (uint32_t(rom[off + 2]) << 8)
                                       |  uint32_t(rom[off + 3]);
                            float f; std::memcpy(&f, &v, 4); return f;
                        }
                        case 7: {
                            float f; std::memcpy(&f, rom + off, 4); return f;
                        }
                        case 11: {
                            uint64_t v = 0;
                            std::memcpy(&v, rom + off, 8);
                            return double(v);
                        }
                        case 13: {
                            double f; std::memcpy(&f, rom + off, 8); return f;
                        }
                        case 1: case 8: case 9: {
                            uint8_t v = rom[off];
                            return sgn ? double(int8_t(v)) : double(v);
                        }
                        default: {
                            switch (cellBytes) {
                            case 1: {
                                uint8_t v = rom[off];
                                return sgn ? double(int8_t(v)) : double(v);
                            }
                            case 2: {
                                uint16_t v = qFromLittleEndian<uint16_t>(rom + off);
                                return sgn ? double(int16_t(v)) : double(v);
                            }
                            case 4: {
                                uint32_t v = qFromLittleEndian<uint32_t>(rom + off);
                                return sgn ? double(int32_t(v)) : double(v);
                            }
                            default: {
                                uint8_t v = rom[off];
                                return sgn ? double(int8_t(v)) : double(v);
                            }
                            }
                        }
                        }
                    };

                    QVector<double> rawVals;
                    rawVals.reserve(count);
                    for (int i = 0; i < count; ++i)
                        rawVals.append(decodeOne(startOff + i * cellBytes));

                    ax.fixedValues = rawVals;
                    ax.ptsCount = count;
                    ax.ptsDataSize = cellBytes;
                };
                readAxisBreakpoints(m.xAxis, m.dimensions.x);
                readAxisBreakpoints(m.yAxis, m.dimensions.y);
            }

            ver.maps = translatedMaps;
            ver.baseAddress = baseAddr;

            for (const auto &w : rv.warnings)
                result.warnings.append(w);

            result.versions.append(std::move(ver));
        }
    }

    return result;
}

}
