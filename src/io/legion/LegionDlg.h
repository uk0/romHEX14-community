/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * LegionDlg — UI shell for the LEGION crowd-tuned verdict pipeline.
 *
 * Two-phase flow:
 *   Phase 1 (clusters): user sees auto-detected intent clusters (stage-like,
 *     dpf-like, mixed, …) discovered by Jaccard-clustering similar files
 *     from the SimilarityIndex.  User picks one or more clusters to feed in.
 *   Phase 2 (verdicts): per-cell aggregated deltas presented as tagged
 *     verdicts (Unanimous / StrongConsensus / Majority / Contested /
 *     Heretic).  Filtered by min-consensus slider, sortable, selectable.
 *
 * "We are many." — Mk 5:9.
 *
 * Scaffolding only at LEGION.5 — the actual voice harvest is synchronous
 * here; the worker thread + progress UI lands in LEGION.8.  The submit
 * workflow + undo are LEGION.7.  Per-verdict preview pane is LEGION.6.
 */

#pragma once

#include "LegionTypes.h"

#include <QByteArray>
#include <QDialog>
#include <QString>
#include <QVector>

class QLabel;
class QPushButton;
class QSlider;
class QProgressBar;
class QStackedWidget;
class QThread;
class QTimer;
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

    // Worker thread (LEGION.8).
    QThread             *m_thread     = nullptr;
    LegionHarvestWorker *m_worker     = nullptr;
    QTimer              *m_taglineTimer = nullptr;
    int                  m_taglineIdx = 0;
};

}   // namespace legion
