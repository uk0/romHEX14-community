/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * HexCompareDlg — side-by-side hex viewer for two open projects.
 *
 * Each pane is a HexWidget bound to one project's bytes; differences are
 * highlighted automatically via setComparisonData() against the opposite
 * pane.  Vertical scroll is cross-wired through HexWidget::scrollSynced
 * → ::syncScrollTo so both panes always show the same byte offset.
 *
 * The dialog is read-only — edits go through the normal project views.
 *
 * Replaces the Pro-only stub previously living at src/stubs/hexcomparedlg.h.
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QPointer>

class HexWidget;
class Project;
class QComboBox;
class QLabel;

class HexCompareDlg : public QDialog {
    Q_OBJECT
public:
    HexCompareDlg(const QList<Project *> &projects,
                  Project *initialLeft,
                  QWidget *parent = nullptr,
                  Project *initialRight = nullptr);

private slots:
    void onLeftPicked(int index);
    void onRightPicked(int index);
    void jumpToCluster(int index);
    void onFirst();
    void onPrev();
    void onNext();
    void onLast();

protected:
    void showEvent(QShowEvent *e) override;

private:
    void buildUi();
    void rebindPanes();
    void updateDiffCount();
    void recomputeClusters();   // contiguous-diff regions, sorted by size

    struct DiffCluster {
        int start;
        int length;
        int diffBytes;          // actual differing bytes inside [start..start+length)
        double density() const  // 0..1, share of bytes that actually differ
        { return length > 0 ? double(diffBytes) / double(length) : 0.0; }
        double score() const    // ranking signal — small + dense wins over big + sparse
        { return double(diffBytes) * density(); }
    };

    QList<QPointer<Project>> m_projects;
    QPointer<Project>        m_left;
    QPointer<Project>        m_right;

    QComboBox *m_cboLeft   = nullptr;
    QComboBox *m_cboRight  = nullptr;
    HexWidget *m_hexLeft   = nullptr;
    HexWidget *m_hexRight  = nullptr;
    QLabel    *m_status    = nullptr;
    QLabel    *m_clusterLbl = nullptr;
    bool       m_syncGuard = false;

    QVector<DiffCluster> m_clusters;
    int                  m_currentCluster = -1;
    bool                 m_initialJumpDone = false;
};
