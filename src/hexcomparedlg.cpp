/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "hexcomparedlg.h"
#include "hexwidget.h"
#include "project.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

HexCompareDlg::HexCompareDlg(const QList<Project *> &projects,
                             Project *initialLeft,
                             QWidget *parent,
                             Project *initialRight)
    : QDialog(parent)
{
    setWindowTitle(tr("Compare hex — side by side"));
    setMinimumSize(1100, 650);

    m_projects.reserve(projects.size());
    for (Project *p : projects) m_projects.append(p);

    // Defaults: caller-provided pair if both given, otherwise active on
    // the left and the first different project on the right.
    if (initialLeft) m_left = initialLeft;
    else if (!m_projects.isEmpty()) m_left = m_projects.first();

    if (initialRight && initialRight != initialLeft) {
        m_right = initialRight;
    } else {
        for (const auto &p : m_projects) {
            if (p && p != m_left) { m_right = p; break; }
        }
    }
    if (!m_right && m_projects.size() >= 1) m_right = m_left;

    buildUi();
    rebindPanes();
}

namespace {
// A "diff cluster" is a contiguous run of differing bytes, allowing tiny
// gaps so that a 64-byte calibration table peppered with matching bytes
// still shows up as one region rather than dozens of singletons.
constexpr int kClusterGap = 16;
constexpr int kMinClusterLength = 4;   // singletons / pairs are noise
} // namespace

void HexCompareDlg::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── Top row: project pickers ──
    auto *pickers = new QHBoxLayout();
    pickers->setSpacing(8);

    m_cboLeft  = new QComboBox();
    m_cboRight = new QComboBox();
    int idxL = 0, idxR = 0;
    for (int i = 0; i < m_projects.size(); ++i) {
        Project *p = m_projects[i].data();
        if (!p) continue;
        const QString label = p->name.isEmpty()
            ? (p->filePath.isEmpty() ? tr("(unnamed)")
                                     : QFileInfo(p->filePath).fileName())
            : p->name;
        m_cboLeft ->addItem(label, QVariant(i));
        m_cboRight->addItem(label, QVariant(i));
        if (p == m_left)  idxL = m_cboLeft ->count() - 1;
        if (p == m_right) idxR = m_cboRight->count() - 1;
    }
    m_cboLeft ->setCurrentIndex(idxL);
    m_cboRight->setCurrentIndex(idxR);

    auto *lblL = new QLabel(tr("Left:"));
    auto *lblR = new QLabel(tr("Right:"));
    pickers->addWidget(lblL);
    pickers->addWidget(m_cboLeft, 1);
    pickers->addSpacing(20);
    pickers->addWidget(lblR);
    pickers->addWidget(m_cboRight, 1);
    root->addLayout(pickers);

    // ── Cluster nav toolbar ──
    auto *nav = new QHBoxLayout();
    nav->setSpacing(4);
    m_clusterLbl = new QLabel(tr("no clusters"));
    m_clusterLbl->setStyleSheet(QStringLiteral("color:#c9d1d9; font-weight:600;"));
    auto *btnFirst = new QPushButton(QStringLiteral("⏮"));
    auto *btnPrev  = new QPushButton(QStringLiteral("◀"));
    auto *btnNext  = new QPushButton(QStringLiteral("▶"));
    auto *btnLast  = new QPushButton(QStringLiteral("⏭"));
    for (auto *b : {btnFirst, btnPrev, btnNext, btnLast})
        b->setMaximumWidth(36);
    btnFirst->setToolTip(tr("Jump to first difference cluster"));
    btnPrev ->setToolTip(tr("Previous cluster"));
    btnNext ->setToolTip(tr("Next cluster"));
    btnLast ->setToolTip(tr("Jump to last cluster"));
    nav->addWidget(new QLabel(tr("Cluster:")));
    nav->addWidget(btnFirst);
    nav->addWidget(btnPrev);
    nav->addWidget(m_clusterLbl, 0);
    nav->addWidget(btnNext);
    nav->addWidget(btnLast);
    nav->addStretch();
    root->addLayout(nav);
    connect(btnFirst, &QPushButton::clicked, this, &HexCompareDlg::onFirst);
    connect(btnPrev,  &QPushButton::clicked, this, &HexCompareDlg::onPrev);
    connect(btnNext,  &QPushButton::clicked, this, &HexCompareDlg::onNext);
    connect(btnLast,  &QPushButton::clicked, this, &HexCompareDlg::onLast);

    // ── Side-by-side panes ──
    auto *split = new QSplitter(Qt::Horizontal);
    m_hexLeft  = new HexWidget();
    m_hexRight = new HexWidget();
    split->addWidget(m_hexLeft);
    split->addWidget(m_hexRight);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    // ── Status + close ──
    auto *bottom = new QHBoxLayout();
    m_status = new QLabel();
    m_status->setMinimumHeight(28);
    m_status->setTextFormat(Qt::RichText);
    m_status->setWordWrap(true);
    bottom->addWidget(m_status, 1);
    auto *btnClose = new QPushButton(tr("Close"));
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottom->addWidget(btnClose);
    root->addLayout(bottom);

    // Pickers swap panes on change.
    connect(m_cboLeft,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, &HexCompareDlg::onLeftPicked);
    connect(m_cboRight, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &HexCompareDlg::onRightPicked);

    // Synchronized scrolling.  Guard against feedback loops:  while we're
    // forwarding from A to B, ignore B's re-emission back to A.
    connect(m_hexLeft, &HexWidget::scrollSynced, this, [this](int off) {
        if (m_syncGuard) return;
        m_syncGuard = true;
        m_hexRight->syncScrollTo(off);
        m_syncGuard = false;
    });
    connect(m_hexRight, &HexWidget::scrollSynced, this, [this](int off) {
        if (m_syncGuard) return;
        m_syncGuard = true;
        m_hexLeft->syncScrollTo(off);
        m_syncGuard = false;
    });
}

void HexCompareDlg::onLeftPicked(int index)
{
    if (index < 0 || index >= m_cboLeft->count()) return;
    const int projIdx = m_cboLeft->itemData(index).toInt();
    if (projIdx >= 0 && projIdx < m_projects.size())
        m_left = m_projects[projIdx];
    rebindPanes();
}

void HexCompareDlg::onRightPicked(int index)
{
    if (index < 0 || index >= m_cboRight->count()) return;
    const int projIdx = m_cboRight->itemData(index).toInt();
    if (projIdx >= 0 && projIdx < m_projects.size())
        m_right = m_projects[projIdx];
    rebindPanes();
}

void HexCompareDlg::rebindPanes()
{
    const QByteArray L = m_left  ? m_left ->currentData : QByteArray();
    const QByteArray R = m_right ? m_right->currentData : QByteArray();
    const QByteArray Lorig = m_left  ? m_left ->originalData : QByteArray();
    const QByteArray Rorig = m_right ? m_right->originalData : QByteArray();

    if (!L.isEmpty()) m_hexLeft ->loadData(L, Lorig, 0);
    else              m_hexLeft ->loadData(QByteArray(), 0);
    if (!R.isEmpty()) m_hexRight->loadData(R, Rorig, 0);
    else              m_hexRight->loadData(QByteArray(), 0);

    // Cross-set comparison bytes so each pane highlights its own deltas
    // against the OTHER project (not just against its own original).
    m_hexLeft ->setComparisonData(R);
    m_hexRight->setComparisonData(L);

    updateDiffCount();
    recomputeClusters();
    // The initial jump is fired from showEvent — when this runs from the
    // constructor the dialog isn't laid out yet so syncScrollTo's
    // verticalScrollBar->maximum() is 0 and the scroll silently no-ops.
}

void HexCompareDlg::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    if (m_initialJumpDone) return;
    m_initialJumpDone = true;
    // Run AFTER the show-event has been processed and the panes have
    // had a chance to lay out + populate their scrollbar ranges.
    QTimer::singleShot(0, this, [this]() {
        if (!m_clusters.isEmpty()) jumpToCluster(0);
    });
}

void HexCompareDlg::recomputeClusters()
{
    m_clusters.clear();
    m_currentCluster = -1;
    if (!m_left || !m_right) { if (m_clusterLbl) m_clusterLbl->setText(tr("—")); return; }
    const QByteArray &a = m_left->currentData;
    const QByteArray &b = m_right->currentData;
    const int n = qMin(a.size(), b.size());
    if (n <= 0) { if (m_clusterLbl) m_clusterLbl->setText(tr("—")); return; }
    const auto *pa = reinterpret_cast<const uint8_t *>(a.constData());
    const auto *pb = reinterpret_cast<const uint8_t *>(b.constData());

    int runStart = -1, lastDiff = -1, runDiffs = 0;
    auto emitRun = [&]() {
        if (runStart < 0) return;
        const int len = lastDiff - runStart + 1;
        if (len >= kMinClusterLength)
            m_clusters.append({runStart, len, runDiffs});
    };
    for (int i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) {
            if (runStart < 0) { runStart = i; lastDiff = i; runDiffs = 1; }
            else if (i - lastDiff <= kClusterGap) {
                lastDiff = i; ++runDiffs;
            } else {
                emitRun();
                runStart = i; lastDiff = i; runDiffs = 1;
            }
        }
    }
    emitRun();

    // Rank by score (diffBytes × density) — favours dense calibration
    // tables over sparse encrypted blocks even when the block is bigger.
    std::sort(m_clusters.begin(), m_clusters.end(),
              [](const DiffCluster &x, const DiffCluster &y) {
        return x.score() > y.score();
    });
    if (m_clusterLbl) {
        m_clusterLbl->setText(m_clusters.isEmpty()
            ? tr("0 / 0")
            : tr("- / %1").arg(m_clusters.size()));
    }
}

void HexCompareDlg::jumpToCluster(int index)
{
    if (index < 0 || index >= m_clusters.size()) return;
    m_currentCluster = index;
    const auto &c = m_clusters[index];
    if (m_clusterLbl) {
        m_clusterLbl->setText(tr("%1 / %2  ·  0x%3  (%4 B, %5% changed)")
            .arg(index + 1).arg(m_clusters.size())
            .arg(c.start, 0, 16).arg(c.length)
            .arg(c.density() * 100.0, 0, 'f', 1));
    }
    m_syncGuard = true;
    m_hexLeft ->syncScrollTo(c.start);
    m_hexRight->syncScrollTo(c.start);
    m_syncGuard = false;
}

void HexCompareDlg::onFirst() { jumpToCluster(0); }
void HexCompareDlg::onLast()  { jumpToCluster(m_clusters.size() - 1); }
void HexCompareDlg::onPrev()
{
    if (m_currentCluster > 0) jumpToCluster(m_currentCluster - 1);
}
void HexCompareDlg::onNext()
{
    if (m_currentCluster + 1 < m_clusters.size())
        jumpToCluster(m_currentCluster + 1);
}

void HexCompareDlg::updateDiffCount()
{
    if (!m_left || !m_right) {
        m_status->setText(tr("Pick two projects to compare."));
        return;
    }
    const QByteArray &a = m_left->currentData;
    const QByteArray &b = m_right->currentData;
    if (a.size() != b.size()) {
        m_status->setText(tr("Size mismatch — Left=%1 B, Right=%2 B "
                              "(byte-by-byte diff disabled).")
                              .arg(a.size()).arg(b.size()));
        return;
    }
    int diff = 0;
    const int n = a.size();
    const auto *pa = reinterpret_cast<const uint8_t *>(a.constData());
    const auto *pb = reinterpret_cast<const uint8_t *>(b.constData());
    for (int i = 0; i < n; ++i) if (pa[i] != pb[i]) ++diff;
    const double diffPct  = n > 0 ? (double(diff) * 100.0 / double(n)) : 0.0;
    const double matchPct = 100.0 - diffPct;
    QString color;
    if      (matchPct >= 95.0) color = QStringLiteral("#3fb950");   // green
    else if (matchPct >= 70.0) color = QStringLiteral("#d29922");   // amber
    else                       color = QStringLiteral("#f85149");   // red
    m_status->setTextFormat(Qt::RichText);
    m_status->setText(QStringLiteral(
        "<b>Byte-level match:</b> "
        "<span style='color:%1; font-size:14pt; font-weight:700;'>%2%</span>"
        "  <span style='color:#8b949e;'>· %3 differing bytes of %4 ·"
        " the Find-Similar % is an n-gram fingerprint, not byte identity</span>")
        .arg(color).arg(matchPct, 0, 'f', 2).arg(diff).arg(n));
}
