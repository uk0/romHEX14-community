/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Catalog Tune Suggestions dialog — UI shell for the catalog-driven tune
 * suggestion pipeline.
 *
 * Two-phase flow:
 *   Phase 1 (groups): scans the SimilarityIndex catalog for similar files,
 *     auto-groups them by intent (Jaccard clustering over changed addresses).
 *   Phase 2 (suggestions): per-cell aggregated deltas, tagged by agreement
 *     (All agree / Strong / Majority / Disputed / Outlier).  Filterable by
 *     min-consensus slider; user ticks which to apply.
 */

#pragma once

#include "LegionTypes.h"

#include <QByteArray>
#include <QDialog>
#include <QHash>
#include <QString>
#include <QVector>

class QLabel;
class QPushButton;
class QSlider;
class QProgressBar;
class QStackedWidget;
class QThread;
class QTreeWidget;
class QTreeWidgetItem;
class Project;

namespace legion {

class LegionHarvestWorker;
class LegionPreviewWidget;

class LegionDlg : public QDialog {
    Q_OBJECT
public:
    LegionDlg(Project *userProject, QWidget *parent = nullptr);
    ~LegionDlg() override;

    /// After dialog accepts, caller reads the verdicts the user marked
    /// for submission.  Empty if cancelled or none selected.
    QVector<LegionVerdict> selectedVerdicts() const { return m_selected; }

private slots:
    void onHarvest();
    void onHarvestProgress(int processed, int total, const QString &cur);
    void onHarvestFinished(QVector<LegionVoice> voices, bool cancelled);
    void onCancelHarvest();
    void onClusterRowChanged();
    void onClusterAdvance();
    void onBackToClusters();
    void onMinConsensusChanged(int v);
    void onVerdictRowChanged();
    void onSubmit();

private:
    void buildUi();
    void populateClusters();
    void populateVerdicts();
    void setHarvestingState(bool running);
    QString voiceShortName(int voiceIdx) const;

    // ── data ──
    Project              *m_project = nullptr;
    QByteArray            m_baseline;
    QVector<LegionVoice>  m_voices;
    QVector<VoiceCluster> m_clusters;
    QVector<LegionVerdict> m_verdicts;
    QVector<LegionVerdict> m_selected;
    int                   m_activeClusterIdx = -1;

    // #8: remember manual check overrides (verdict index → checked) so the
    // consensus-filter slider, which rebuilds the verdict rows, does not
    // discard the user's hand-picked selection.  m_populating suppresses the
    // itemChanged signal while we set states programmatically.
    QHash<int, bool>      m_manualCheck;
    bool                  m_populating = false;

    // ── widgets ──
    QStackedWidget *m_stack       = nullptr;
    // Phase 1 — cluster picker.
    QLabel         *m_lblTagline  = nullptr;
    QLabel         *m_lblSummary  = nullptr;
    QPushButton    *m_btnHarvest  = nullptr;
    QPushButton    *m_btnCancel   = nullptr;
    QProgressBar   *m_progress    = nullptr;
    QLabel         *m_lblProgress = nullptr;
    QTreeWidget    *m_treeCluster = nullptr;
    QPushButton    *m_btnNext     = nullptr;
    // Phase 2 — verdicts.
    QLabel         *m_lblCluster  = nullptr;
    QSlider        *m_sliderMin   = nullptr;
    QLabel         *m_lblMin      = nullptr;
    QTreeWidget    *m_treeVerdict = nullptr;
    QPushButton    *m_btnBack     = nullptr;
    QPushButton    *m_btnSubmit   = nullptr;
    QLabel         *m_lblStatus   = nullptr;
    LegionPreviewWidget *m_preview = nullptr;

    // Worker thread.
    QThread             *m_thread     = nullptr;
    LegionHarvestWorker *m_worker     = nullptr;
};

}   // namespace legion
