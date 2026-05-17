/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Per-verdict preview pane for the LEGION dialog.
 *
 * Picks a renderer by VerdictKind:
 *   Scalar        — large delta-number readout + error bar
 *   Curve         — polyline of per-cell meanDelta with ±stdDev band
 *   SmallMap      — colored heatmap of per-cell meanDelta (auto-scaled)
 *   LargeMap      — same heatmap, denser cells
 * Below the chart, a "voices" list shows which cluster members contributed
 * (filename + their delta at the densest cell, when available).
 */

#pragma once

#include "LegionTypes.h"

#include <QByteArray>
#include <QFrame>
#include <QVector>
#include <QWidget>

class QLabel;
class QTreeWidget;

namespace legion {

// Inner canvas — handles the actual drawing of the visualization.
class LegionPreviewCanvas : public QFrame {
    Q_OBJECT
public:
    explicit LegionPreviewCanvas(QWidget *parent = nullptr);
    void setVerdict(const LegionVerdict *v);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    void paintScalar(QPainter &p);
    void paintCurve (QPainter &p);
    void paintMap   (QPainter &p);
    const LegionVerdict *m_verdict = nullptr;
};

class LegionPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit LegionPreviewWidget(QWidget *parent = nullptr);

    /// Bind backing data — must outlive the widget or be replaced before
    /// the verdict is shown.
    void setContext(const QVector<LegionVoice> *voices,
                    const QByteArray *userBaseline);

    /// Show the given verdict; clears the pane if nullptr.
    void showVerdict(const LegionVerdict *v);

private:
    void updateVoiceList();
    int  densestCellIndex() const;

    const QVector<LegionVoice> *m_voices   = nullptr;
    const QByteArray           *m_baseline = nullptr;
    const LegionVerdict        *m_verdict  = nullptr;

    QLabel              *m_lblTitle   = nullptr;
    QLabel              *m_lblMeta    = nullptr;
    LegionPreviewCanvas *m_canvas     = nullptr;
    QTreeWidget         *m_treeVoices = nullptr;
};

}   // namespace legion
