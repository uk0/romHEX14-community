/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * LegionPreviewWidget — per-suggestion preview pane (heatmap + sources).
 */

#include "io/legion/LegionPreviewWidget.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QObject>
#include <QPainter>
#include <QPen>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace legion {

namespace {

QString tagName(VerdictTag t)
{
    switch (t) {
    case VerdictTag::Unanimous:       return QObject::tr("All agree");
    case VerdictTag::StrongConsensus: return QObject::tr("Strong");
    case VerdictTag::Majority:        return QObject::tr("Majority");
    case VerdictTag::Contested:       return QObject::tr("Disputed");
    case VerdictTag::Heretic:         return QObject::tr("Outlier");
    case VerdictTag::Checksum:        return QObject::tr("Checksum");
    case VerdictTag::KillRegion:      return QObject::tr("Erase");
    }
    return QStringLiteral("?");
}

QString kindName(VerdictKind k)
{
    switch (k) {
    case VerdictKind::Scalar:   return QObject::tr("Scalar");
    case VerdictKind::Curve:    return QObject::tr("Curve");
    case VerdictKind::SmallMap: return QObject::tr("Map");
    case VerdictKind::LargeMap: return QObject::tr("Large map");
    }
    return QStringLiteral("?");
}

// Colorize a delta value relative to its absolute peak:
//   negative → red ramp, zero → near-black, positive → green ramp.
QColor heatColor(double v, double peakAbs)
{
    if (peakAbs <= 1e-9 || !std::isfinite(v)) return QColor(40, 40, 40);
    const double t = std::max(-1.0, std::min(1.0, v / peakAbs));
    if (t < 0) {
        const int r = int(60 + (-t) * 195);
        return QColor(r, 30, 35);
    }
    const int g = int(60 + t * 175);
    return QColor(25, g, 50);
}

}   // anonymous

// ─── LegionPreviewCanvas ────────────────────────────────────────────────────

LegionPreviewCanvas::LegionPreviewCanvas(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setMinimumHeight(160);
}

void LegionPreviewCanvas::setVerdict(const LegionVerdict *v)
{
    m_verdict = v;
    update();
}

void LegionPreviewCanvas::paintEvent(QPaintEvent *e)
{
    QFrame::paintEvent(e);
    if (!m_verdict || m_verdict->cells.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    switch (m_verdict->kind) {
    case VerdictKind::Scalar:   paintScalar(p); break;
    case VerdictKind::Curve:    paintCurve (p); break;
    case VerdictKind::SmallMap: paintMap   (p); break;
    case VerdictKind::LargeMap: paintMap   (p); break;
    }
}

void LegionPreviewCanvas::paintScalar(QPainter &p)
{
    const auto &c = m_verdict->cells.first();
    const QRect r = rect().adjusted(8, 8, -8, -8);

    QFont big = p.font();
    big.setPointSize(28);
    big.setBold(true);
    p.setFont(big);
    p.setPen(QColor(c.meanDelta >= 0 ? "#3fb950" : "#f85149"));
    const QString delta = QStringLiteral("%1%2")
        .arg(c.meanDelta >= 0 ? "+" : "")
        .arg(c.meanDelta, 0, 'f', 2);
    p.drawText(r, Qt::AlignHCenter | Qt::AlignTop, delta);

    QFont small = p.font();
    small.setPointSize(9);
    small.setBold(false);
    p.setFont(small);
    p.setPen(QColor("#8b949e"));
    const QString sub = QStringLiteral("± %1   ·   n=%2")
        .arg(c.stdDevDelta, 0, 'f', 2)
        .arg(c.sampleCount);
    p.drawText(r, Qt::AlignHCenter | Qt::AlignBottom, sub);
}

void LegionPreviewCanvas::paintCurve(QPainter &p)
{
    const auto &cells = m_verdict->cells;
    const int n = cells.size();
    if (n < 2) { paintScalar(p); return; }

    double minV = cells[0].meanDelta, maxV = cells[0].meanDelta;
    for (const auto &c : cells) {
        const double lo = c.meanDelta - c.stdDevDelta;
        const double hi = c.meanDelta + c.stdDevDelta;
        if (lo < minV) minV = lo;
        if (hi > maxV) maxV = hi;
    }
    if (maxV - minV < 1e-9) { maxV = minV + 1.0; }

    const QRect r = rect().adjusted(20, 14, -20, -14);
    auto xAt = [&](int i) {
        return r.left() + int(double(i) * (r.width() - 1) / double(n - 1));
    };
    auto yAt = [&](double v) {
        const double t = (v - minV) / (maxV - minV);
        return r.bottom() - int(t * (r.height() - 1));
    };

    // Zero line.
    if (minV < 0 && maxV > 0) {
        p.setPen(QPen(QColor("#30363d"), 1, Qt::DashLine));
        const int y0 = yAt(0.0);
        p.drawLine(r.left(), y0, r.right(), y0);
    }

    // ±stdDev band.
    QPolygonF bandTop, bandBot;
    for (int i = 0; i < n; ++i) {
        bandTop << QPointF(xAt(i), yAt(cells[i].meanDelta + cells[i].stdDevDelta));
        bandBot << QPointF(xAt(i), yAt(cells[i].meanDelta - cells[i].stdDevDelta));
    }
    QPolygonF band = bandTop;
    for (int i = bandBot.size() - 1; i >= 0; --i) band << bandBot[i];
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(88, 166, 255, 40));
    p.drawPolygon(band);

    // Mean line.
    QPolygonF line;
    for (int i = 0; i < n; ++i) line << QPointF(xAt(i), yAt(cells[i].meanDelta));
    p.setPen(QPen(QColor("#58a6ff"), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);

    // Axis labels.
    QFont f = p.font(); f.setPointSize(8); p.setFont(f);
    p.setPen(QColor("#6e7681"));
    p.drawText(rect().adjusted(2, 2, -2, -2), Qt::AlignLeft | Qt::AlignTop,
        QStringLiteral("max %1").arg(maxV, 0, 'f', 1));
    p.drawText(rect().adjusted(2, 2, -2, -2), Qt::AlignLeft | Qt::AlignBottom,
        QStringLiteral("min %1").arg(minV, 0, 'f', 1));
}

void LegionPreviewCanvas::paintMap(QPainter &p)
{
    const auto &cells = m_verdict->cells;
    const int n    = cells.size();
    const int rows = std::max(1, m_verdict->rows);
    const int cols = std::max(1, m_verdict->cols);
    if (rows * cols != n) { paintCurve(p); return; }

    double peak = 0.0;
    for (const auto &c : cells) peak = std::max(peak, std::abs(c.meanDelta));
    if (peak < 1e-9) peak = 1.0;

    const QRect r = rect().adjusted(8, 8, -8, -8);
    const int cellW = std::max(1, r.width()  / cols);
    const int cellH = std::max(1, r.height() / rows);
    const int gridW = cellW * cols;
    const int gridH = cellH * rows;
    const int x0    = r.left() + (r.width()  - gridW) / 2;
    const int y0    = r.top()  + (r.height() - gridH) / 2;

    p.setPen(Qt::NoPen);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const auto &c = cells[row * cols + col];
            QColor cellColor = (c.sampleCount > 0)
                ? heatColor(c.meanDelta, peak)
                : QColor(35, 38, 45);          // unsampled
            p.setBrush(cellColor);
            p.drawRect(x0 + col * cellW, y0 + row * cellH, cellW, cellH);
        }
    }
    // Outline.
    p.setPen(QPen(QColor("#30363d"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(x0, y0, gridW, gridH);
}

// ─── LegionPreviewWidget ────────────────────────────────────────────────────

LegionPreviewWidget::LegionPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    m_lblTitle = new QLabel(tr("(select a suggestion)"));
    QFont titleFont = m_lblTitle->font();
    titleFont.setBold(true);
    m_lblTitle->setFont(titleFont);
    root->addWidget(m_lblTitle);

    m_lblMeta = new QLabel();
    m_lblMeta->setWordWrap(true);
    root->addWidget(m_lblMeta);

    m_canvas = new LegionPreviewCanvas();
    root->addWidget(m_canvas, 1);

    auto *voicesLabel = new QLabel(tr("Contributing files"));
    root->addWidget(voicesLabel);

    m_treeVoices = new QTreeWidget();
    m_treeVoices->setColumnCount(3);
    m_treeVoices->setHeaderLabels({tr("Source"), tr("Match %"),
                                   tr("Δ at densest cell")});
    m_treeVoices->setRootIsDecorated(false);
    m_treeVoices->setAlternatingRowColors(true);
    m_treeVoices->header()->setStretchLastSection(true);
    m_treeVoices->setMinimumHeight(120);
    root->addWidget(m_treeVoices, 0);
}

void LegionPreviewWidget::setContext(const QVector<LegionVoice> *voices,
                                     const QByteArray *userBaseline)
{
    m_voices   = voices;
    m_baseline = userBaseline;
}

void LegionPreviewWidget::showVerdict(const LegionVerdict *v)
{
    m_verdict = v;
    m_canvas->setVerdict(v);
    if (!v) {
        m_lblTitle->setText(tr("(select a suggestion)"));
        m_lblMeta->clear();
        m_treeVoices->clear();
        return;
    }
    m_lblTitle->setText(tr("[%1]  0x%2..0x%3   %4 %5×%6")
        .arg(tagName(v->tag))
        .arg(v->startAddr, 0, 16)
        .arg(v->endAddr,   0, 16)
        .arg(kindName(v->kind))
        .arg(v->rows).arg(v->cols));
    m_lblMeta->setText(tr("Cell size: %1 B · Cells: %2 · "
                          "Max samples: %3 · Consensus: %4")
        .arg(v->cellSize).arg(v->cells.size())
        .arg(v->maxSampleCount)
        .arg(v->consensusStrength, 0, 'f', 3));
    updateVoiceList();
}

int LegionPreviewWidget::densestCellIndex() const
{
    if (!m_verdict || m_verdict->cells.isEmpty()) return -1;
    int best = 0;
    for (int i = 1; i < m_verdict->cells.size(); ++i) {
        if (m_verdict->cells[i].sampleCount >
            m_verdict->cells[best].sampleCount) best = i;
    }
    return best;
}

void LegionPreviewWidget::updateVoiceList()
{
    m_treeVoices->clear();
    if (!m_verdict || !m_voices) return;

    const int dCell = densestCellIndex();
    const int cellSize = std::max(1, m_verdict->cellSize);
    const uint32_t cellStart = (dCell >= 0)
        ? m_verdict->startAddr + uint32_t(dCell * cellSize) : 0;

    for (int vi : m_verdict->contributingVoices) {
        if (vi < 0 || vi >= m_voices->size()) continue;
        const auto &voice = (*m_voices)[vi];
        auto *it = new QTreeWidgetItem(m_treeVoices);
        // #2: attribute the contribution to a concrete .ols version.
        QString label = QFileInfo(voice.sourcePath).fileName();
        if (!voice.versionLabel.isEmpty())
            label += QStringLiteral(" › ") + voice.versionLabel;
        else if (voice.versionIndex >= 0)
            label += QStringLiteral(" › v%1").arg(voice.versionIndex);
        it->setText(0, label);
        it->setToolTip(0, voice.sourcePath +
            (voice.versionLabel.isEmpty()
                ? QString()
                : QStringLiteral("\nVersion: ") + voice.versionLabel));
        it->setText(1, QStringLiteral("%1%%").arg(voice.similarity));

        // Voice's delta at the densest cell — assemble the cell-size bytes
        // into orig/mod integer VALUES honouring endianness, then report
        // their difference (#9: previously summed per-byte differences,
        // which is wrong for cellSize 2/4 and ignores byte order, so it did
        // not match the delta the verdict actually applies).
        QString deltaTxt = QStringLiteral("—");
        const int nBytes = std::min(cellSize, 8);
        if (dCell >= 0) {
            uint8_t origBuf[8] = {0}, modBuf[8] = {0};
            bool covered = true;
            for (int b = 0; b < nBytes; ++b) {
                const uint32_t addr = cellStart + uint32_t(b);
                int found = -1;
                int lo = 0, hi = voice.regions.size() - 1;
                while (lo <= hi) {
                    const int mid = (lo + hi) / 2;
                    const auto &r = voice.regions[mid];
                    if (addr < r.startAddr) hi = mid - 1;
                    else if (addr > r.endAddr) lo = mid + 1;
                    else { found = mid; break; }
                }
                if (found < 0) { covered = false; break; }
                const auto &r = voice.regions[found];
                const int off = int(addr - r.startAddr);
                if (off < 0 || off >= r.originalBytes.size()
                    || off >= r.modifiedBytes.size()) {
                    covered = false; break;
                }
                origBuf[b] = uint8_t(r.originalBytes[off]);
                modBuf[b]  = uint8_t(r.modifiedBytes[off]);
            }
            if (covered) {
                const bool be = m_verdict->bigEndian;
                int64_t origVal = 0, modVal = 0;
                for (int b = 0; b < nBytes; ++b) {
                    const int shift = be ? (nBytes - 1 - b) : b;
                    origVal |= int64_t(origBuf[b]) << (8 * shift);
                    modVal  |= int64_t(modBuf[b])  << (8 * shift);
                }
                const int64_t delta = modVal - origVal;
                deltaTxt = QStringLiteral("%1%2")
                    .arg(delta >= 0 ? "+" : "").arg(delta);
            }
        }
        it->setText(2, deltaTxt);
    }
}

}   // namespace legion
