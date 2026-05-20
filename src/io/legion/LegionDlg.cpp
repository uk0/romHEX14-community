/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Catalog Tune Suggestions dialog implementation.
 *
 * Two-phase flow: scan the catalog for similar files, then group them by
 * intent and aggregate per-cell suggestions.  Harvest runs on a worker
 * thread (see LegionHarvestWorker); the main work here is presenting
 * groups and verdicts to the user.
 */

#include "io/legion/LegionDlg.h"
#include "io/legion/Legion.h"
#include "io/legion/LegionHarvestWorker.h"
#include "io/legion/LegionPreviewWidget.h"

#include "project.h"

#include <QApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace legion {

namespace {

constexpr int kMinPercent     = 85;     // catalog similarity floor (% match)
constexpr int kMaxVoices      = 30;     // hard cap on catalog scan
constexpr double kJaccardMin  = 0.50;   // intent-clustering threshold
constexpr double kLocalSimMin = 0.90;   // per-region local hamming gate

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

// #7: the consensus column shows text like "9.9%"/"100.0%", which a plain
// QTreeWidget sorts lexicographically ("9.9%" > "100.0%").  This item sorts
// the consensus column numerically via a double stashed in Qt::UserRole;
// every other column keeps the default text comparison.
constexpr int kConsensusColumn = 7;
class VerdictItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int col = treeWidget() ? treeWidget()->sortColumn() : 0;
        if (col == kConsensusColumn) {
            return data(col, Qt::UserRole).toDouble()
                 < other.data(col, Qt::UserRole).toDouble();
        }
        return QTreeWidgetItem::operator<(other);
    }
};

}   // anonymous

// ─── ctor / dtor ────────────────────────────────────────────────────────────

LegionDlg::LegionDlg(Project *userProject, QWidget *parent)
    : QDialog(parent), m_project(userProject)
{
    setWindowTitle(tr("Catalog Tune Suggestions"));
    setMinimumSize(900, 600);
    if (m_project) {
        // #3: baseline must follow the version the user is actually looking
        // at.  restoreVersion() writes only currentData, so scanning against
        // originalData ignored the user's version choice.  Prefer
        // currentData; fall back to originalData only if currentData is empty.
        m_baseline = !m_project->currentData.isEmpty()
                   ? m_project->currentData : m_project->originalData;
    }
    buildUi();
}

LegionDlg::~LegionDlg()
{
    // If a harvest is still running, ask it to stop and wait briefly.
    // The worker is parented to the QThread which deleteLaters itself on
    // QThread::finished, so we just need the thread to exit before our
    // QObject parent gets torn down.
    if (m_thread && m_thread->isRunning()) {
        if (m_worker) m_worker->cancel();
        m_thread->quit();
        m_thread->wait(2000);
    }
}

// ─── UI ─────────────────────────────────────────────────────────────────────

void LegionDlg::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    m_lblTagline = new QLabel(tr("Find similar tunes in your catalog and "
                                 "review the changes they share."));
    m_lblTagline->setWordWrap(true);
    root->addWidget(m_lblTagline);

    m_stack = new QStackedWidget();
    root->addWidget(m_stack, 1);

    // ── Phase 1: group picker ────────────────────────────────────────────
    auto *page1 = new QWidget();
    auto *p1 = new QVBoxLayout(page1);
    p1->setContentsMargins(0, 0, 0, 0);
    p1->setSpacing(8);

    m_lblSummary = new QLabel(tr("Scan the catalog for similar tunes "
                                 "(≥%1%% match) and group them by intent.")
                              .arg(kMinPercent));
    m_lblSummary->setWordWrap(true);
    p1->addWidget(m_lblSummary);

    auto *p1Btn = new QHBoxLayout();
    m_btnHarvest = new QPushButton(tr("Scan catalog"));
    m_btnCancel = new QPushButton(tr("Cancel"));
    m_btnCancel->hide();
    p1Btn->addWidget(m_btnHarvest);
    p1Btn->addWidget(m_btnCancel);
    p1Btn->addStretch();
    p1->addLayout(p1Btn);

    m_progress = new QProgressBar();
    m_progress->setRange(0, 0);
    m_progress->setMaximumHeight(8);
    m_progress->setTextVisible(false);
    m_progress->hide();
    p1->addWidget(m_progress);

    m_lblProgress = new QLabel();
    m_lblProgress->setWordWrap(true);
    m_lblProgress->hide();
    p1->addWidget(m_lblProgress);

    m_treeCluster = new QTreeWidget();
    m_treeCluster->setColumnCount(5);
    m_treeCluster->setHeaderLabels({tr("Files"), tr("Label"),
                                    tr("Range"), tr("Common addresses"),
                                    tr("Keywords")});
    m_treeCluster->setRootIsDecorated(false);
    m_treeCluster->setAlternatingRowColors(true);
    m_treeCluster->setSortingEnabled(false);
    m_treeCluster->setSelectionMode(QAbstractItemView::SingleSelection);
    p1->addWidget(m_treeCluster, 1);

    auto *p1Foot = new QHBoxLayout();
    p1Foot->addStretch();
    m_btnNext = new QPushButton(tr("Analyze group →"));
    m_btnNext->setEnabled(false);
    p1Foot->addWidget(m_btnNext);
    auto *p1Close = new QPushButton(tr("Close"));
    p1Foot->addWidget(p1Close);
    p1->addLayout(p1Foot);

    m_stack->addWidget(page1);

    // ── Phase 2: per-cell suggestions ────────────────────────────────────
    auto *page2 = new QWidget();
    auto *p2 = new QVBoxLayout(page2);
    p2->setContentsMargins(0, 0, 0, 0);
    p2->setSpacing(8);

    m_lblCluster = new QLabel();
    p2->addWidget(m_lblCluster);

    auto *p2Filter = new QHBoxLayout();
    auto *minTxt = new QLabel(tr("Min consensus:"));
    m_sliderMin = new QSlider(Qt::Horizontal);
    m_sliderMin->setRange(0, 100);
    m_sliderMin->setValue(QSettings()
        .value(QStringLiteral("legion/minConsensusPct"), 30).toInt());
    m_lblMin = new QLabel(QString::number(m_sliderMin->value()) + "%");
    m_lblMin->setMinimumWidth(40);
    p2Filter->addWidget(minTxt);
    p2Filter->addWidget(m_sliderMin, 1);
    p2Filter->addWidget(m_lblMin);
    p2->addLayout(p2Filter);

    auto *splitter = new QSplitter(Qt::Horizontal);

    m_treeVerdict = new QTreeWidget();
    m_treeVerdict->setColumnCount(8);
    m_treeVerdict->setHeaderLabels({tr("Apply"), tr("Agreement"), tr("Address"),
                                    tr("Size"), tr("Kind"), tr("Cells"),
                                    tr("Coverage"), tr("Consensus")});
    m_treeVerdict->setRootIsDecorated(false);
    m_treeVerdict->setAlternatingRowColors(true);
    m_treeVerdict->setSortingEnabled(true);
    m_treeVerdict->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeVerdict->header()->setStretchLastSection(true);
    splitter->addWidget(m_treeVerdict);

    m_preview = new LegionPreviewWidget();
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({560, 340});

    p2->addWidget(splitter, 1);

    m_lblStatus = new QLabel();
    p2->addWidget(m_lblStatus);

    auto *p2Foot = new QHBoxLayout();
    m_btnBack = new QPushButton(tr("← Back to groups"));
    p2Foot->addWidget(m_btnBack);
    p2Foot->addStretch();
    m_btnSubmit = new QPushButton(tr("Apply selected"));
    p2Foot->addWidget(m_btnSubmit);
    auto *p2Close = new QPushButton(tr("Close"));
    p2Foot->addWidget(p2Close);
    p2->addLayout(p2Foot);

    m_stack->addWidget(page2);
    m_stack->setCurrentIndex(0);

    // ── connections ──
    connect(m_btnHarvest, &QPushButton::clicked, this, &LegionDlg::onHarvest);
    connect(m_btnCancel,  &QPushButton::clicked, this, &LegionDlg::onCancelHarvest);
    connect(m_treeCluster, &QTreeWidget::itemSelectionChanged,
            this, &LegionDlg::onClusterRowChanged);
    connect(m_treeCluster, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *, int) { onClusterAdvance(); });
    connect(m_btnNext,  &QPushButton::clicked, this, &LegionDlg::onClusterAdvance);
    connect(m_btnBack,  &QPushButton::clicked, this, &LegionDlg::onBackToClusters);
    connect(m_sliderMin, &QSlider::valueChanged,
            this, &LegionDlg::onMinConsensusChanged);
    connect(m_btnSubmit, &QPushButton::clicked, this, &LegionDlg::onSubmit);
    connect(m_treeVerdict, &QTreeWidget::itemSelectionChanged,
            this, &LegionDlg::onVerdictRowChanged);
    // #8: record manual check toggles so a later filter-rebuild can restore
    // them.  Ignore changes we make ourselves during populateVerdicts().
    connect(m_treeVerdict, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem *it, int col) {
        if (m_populating || col != 0) return;
        const int idx = it->data(0, Qt::UserRole).toInt();
        m_manualCheck[idx] = (it->checkState(0) == Qt::Checked);
    });
    connect(p1Close, &QPushButton::clicked, this, &QDialog::reject);
    connect(p2Close, &QPushButton::clicked, this, &QDialog::reject);
}

// ─── Phase 1: voice harvest + clustering ───────────────────────────────────
//
// Synchronous & capped; LEGION.8 moves this into a worker thread with
// progress.  Reads the catalog index, picks the top kMaxVoices matches at
// ≥kMinPercent overall similarity, imports each via OlsImporter to extract
// "original" (version 0) + "stage1" (highest-numbered version), runs
// detectRegions to build per-voice region lists.

void LegionDlg::onHarvest()
{
    if (!m_project || m_baseline.isEmpty()) {
        QMessageBox::warning(this, tr("Catalog Tune Suggestions"),
            tr("Open a project with a ROM loaded first."));
        return;
    }
    if (m_thread && m_thread->isRunning()) return;   // already scanning

    setHarvestingState(true);

    m_thread = new QThread(this);
    m_worker = new LegionHarvestWorker(m_baseline, m_project->filePath,
                                       kMinPercent, kMaxVoices);
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_worker, &LegionHarvestWorker::run);
    connect(m_worker, &LegionHarvestWorker::progress,
            this,     &LegionDlg::onHarvestProgress);
    connect(m_worker, &LegionHarvestWorker::finished,
            this,     &LegionDlg::onHarvestFinished);
    connect(m_worker, &LegionHarvestWorker::finished,
            m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, this, [this]() {
        m_thread = nullptr;
        m_worker = nullptr;
    });
    m_thread->start();
}

void LegionDlg::onHarvestProgress(int processed, int total,
                                  const QString &cur)
{
    if (total > 0) {
        m_progress->setRange(0, total);
        m_progress->setValue(processed);
    }
    m_lblProgress->setText(tr("%1 / %2  ·  %3")
        .arg(processed).arg(total).arg(QFileInfo(cur).fileName()));
}

void LegionDlg::onHarvestFinished(QVector<LegionVoice> voices, bool cancelled)
{
    setHarvestingState(false);

    if (cancelled) {
        m_lblTagline->setText(tr("Scan cancelled."));
        return;
    }

    m_voices = std::move(voices);
    if (m_voices.isEmpty()) {
        QMessageBox::information(this, tr("Catalog Tune Suggestions"),
            tr("No similar files were found in the catalog for this ROM."));
        m_lblTagline->setText(tr("Find similar tunes in your catalog and "
                                 "review the changes they share."));
        return;
    }
    m_clusters = clusterVoices(m_voices, kJaccardMin);
    m_preview->setContext(&m_voices, &m_baseline);
    populateClusters();
    m_lblTagline->setText(tr("Scan complete. Pick a group to analyze."));
}

void LegionDlg::onCancelHarvest()
{
    if (m_worker) m_worker->cancel();
    m_btnCancel->setEnabled(false);
    m_btnCancel->setText(tr("Cancelling…"));
    m_lblTagline->setText(tr("Cancelling scan…"));
}

void LegionDlg::setHarvestingState(bool running)
{
    m_btnHarvest->setVisible(!running);
    m_btnCancel ->setVisible(running);
    m_btnCancel ->setEnabled(running);
    m_btnCancel ->setText(tr("Cancel"));
    m_progress  ->setVisible(running);
    m_lblProgress->setVisible(running);
    if (running) {
        m_progress->setRange(0, 0);   // busy until first progress signal
        m_lblProgress->setText(tr("Searching catalog…"));
        m_lblTagline->setText(tr("Scanning catalog for similar tunes…"));
    }
}

void LegionDlg::populateClusters()
{
    m_treeCluster->clear();
    for (int ci = 0; ci < m_clusters.size(); ++ci) {
        const auto &c = m_clusters[ci];
        auto *it = new QTreeWidgetItem(m_treeCluster);
        it->setText(0, QString::number(c.voiceIndices.size()));
        it->setText(1, c.label);
        it->setText(2, QStringLiteral("0x%1..0x%2")
            .arg(c.addrRangeMin, 0, 16).arg(c.addrRangeMax, 0, 16));
        it->setText(3, QString::number(c.consensusAddrCount));
        QStringList kws;
        for (auto kit = c.filenameKeywords.constBegin();
             kit != c.filenameKeywords.constEnd(); ++kit) {
            kws << QStringLiteral("%1×%2").arg(kit.key()).arg(kit.value());
        }
        it->setText(4, kws.join(QStringLiteral(", ")));
        it->setData(0, Qt::UserRole, ci);
        if (c.voiceIndices.size() < 2) it->setForeground(0, QColor("#6e7681"));
    }
    if (m_treeCluster->topLevelItemCount() > 0) {
        m_treeCluster->topLevelItem(0)->setSelected(true);
    }
}

void LegionDlg::onClusterRowChanged()
{
    const auto sel = m_treeCluster->selectedItems();
    if (sel.isEmpty()) { m_btnNext->setEnabled(false); return; }
    const int ci = sel.first()->data(0, Qt::UserRole).toInt();
    m_btnNext->setEnabled(ci >= 0 && ci < m_clusters.size()
                          && m_clusters[ci].voiceIndices.size() >= 2);
}

// ─── Phase 2: aggregate + classify ─────────────────────────────────────────

void LegionDlg::onClusterAdvance()
{
    const auto sel = m_treeCluster->selectedItems();
    if (sel.isEmpty()) return;
    const int ci = sel.first()->data(0, Qt::UserRole).toInt();
    if (ci < 0 || ci >= m_clusters.size()) return;
    if (m_clusters[ci].voiceIndices.size() < 2) return;

    m_activeClusterIdx = ci;
    const auto &cluster = m_clusters[ci];
    m_verdicts = aggregate(m_voices, m_baseline, cluster, kLocalSimMin);
    classify(m_verdicts, cluster.voiceIndices.size());
    m_manualCheck.clear();   // #8: new cluster = fresh verdict set

    m_lblCluster->setText(tr("Group: %1 files · %2 · range 0x%3..0x%4")
        .arg(cluster.voiceIndices.size())
        .arg(cluster.label)
        .arg(cluster.addrRangeMin, 0, 16)
        .arg(cluster.addrRangeMax, 0, 16));
    populateVerdicts();
    m_stack->setCurrentIndex(1);
    m_lblTagline->setText(tr("Review the suggested changes. "
                             "Tick the ones you want to apply."));
}

void LegionDlg::populateVerdicts()
{
    m_populating = true;          // #8: suppress itemChanged while we rebuild
    m_treeVerdict->clear();
    m_preview->showVerdict(nullptr);
    if (m_activeClusterIdx < 0 || m_activeClusterIdx >= m_clusters.size())
        return;
    const int totalVoices = m_clusters[m_activeClusterIdx].voiceIndices.size();
    const int minPct = m_sliderMin->value();
    m_treeVerdict->setSortingEnabled(false);

    int kept = 0, total = 0;
    for (const auto &v : m_verdicts) {
        ++total;
        const double pct = v.consensusStrength * 100.0;
        if (pct < minPct) continue;
        const double coverage = totalVoices > 0
            ? double(v.maxSampleCount) / double(totalVoices) : 0.0;

        auto *it = new VerdictItem(m_treeVerdict);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        // Auto-tick anything stronger than Majority — unless the user has
        // manually overridden this verdict (#8), in which case honour that.
        const int verdictIdx = total - 1;
        const bool autoTick = (v.tag == VerdictTag::Unanimous ||
                               v.tag == VerdictTag::StrongConsensus);
        const bool checked = m_manualCheck.value(verdictIdx, autoTick);
        it->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
        it->setText(1, tagName(v.tag));
        it->setText(2, QStringLiteral("0x%1").arg(v.startAddr, 0, 16));
        it->setText(3, QString::number(v.endAddr - v.startAddr + 1));
        it->setText(4, QStringLiteral("%1 %2×%3")
            .arg(kindName(v.kind))
            .arg(v.rows).arg(v.cols));
        it->setText(5, QString::number(v.cells.size()));
        it->setText(6, QStringLiteral("%1/%2")
            .arg(v.maxSampleCount).arg(totalVoices));
        it->setText(7, QStringLiteral("%1%").arg(pct, 0, 'f', 1));
        it->setData(7, Qt::UserRole, pct);   // #7: numeric sort key

        QColor c;
        switch (v.tag) {
        case VerdictTag::Unanimous:       c = QColor("#3fb950"); break;
        case VerdictTag::StrongConsensus: c = QColor("#58a6ff"); break;
        case VerdictTag::Majority:        c = QColor("#d29922"); break;
        case VerdictTag::Contested:       c = QColor("#f85149"); break;
        case VerdictTag::Heretic:         c = QColor("#6e7681"); break;
        default:                          c = QColor("#c9d1d9"); break;
        }
        it->setForeground(1, c);

        it->setData(0, Qt::UserRole, total - 1);   // index back into m_verdicts
        ++kept;
        (void)coverage;
    }
    m_treeVerdict->setSortingEnabled(true);
    m_treeVerdict->sortByColumn(7, Qt::DescendingOrder);
    m_lblStatus->setText(tr("%1 of %2 suggestions shown (≥%3%% consensus)")
                         .arg(kept).arg(total).arg(minPct));
    m_populating = false;   // #8
}

void LegionDlg::onMinConsensusChanged(int v)
{
    m_lblMin->setText(QString::number(v) + "%");
    QSettings().setValue(QStringLiteral("legion/minConsensusPct"), v);
    populateVerdicts();
}

void LegionDlg::onBackToClusters()
{
    m_stack->setCurrentIndex(0);
    m_preview->showVerdict(nullptr);
    m_lblTagline->setText(tr("Pick another group, or close."));
}

void LegionDlg::onVerdictRowChanged()
{
    const auto sel = m_treeVerdict->selectedItems();
    if (sel.isEmpty()) { m_preview->showVerdict(nullptr); return; }
    const int idx = sel.first()->data(0, Qt::UserRole).toInt();
    if (idx >= 0 && idx < m_verdicts.size()) {
        m_preview->showVerdict(&m_verdicts[idx]);
    } else {
        m_preview->showVerdict(nullptr);
    }
}

void LegionDlg::onSubmit()
{
    m_selected.clear();
    for (int i = 0; i < m_treeVerdict->topLevelItemCount(); ++i) {
        auto *it = m_treeVerdict->topLevelItem(i);
        if (it->checkState(0) != Qt::Checked) continue;
        const int idx = it->data(0, Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_verdicts.size()) {
            m_selected.append(m_verdicts[idx]);
        }
    }
    if (m_selected.isEmpty()) {
        QMessageBox::information(this, tr("Catalog Tune Suggestions"),
            tr("Nothing selected to apply."));
        return;
    }
    accept();
}

QString LegionDlg::voiceShortName(int voiceIdx) const
{
    if (voiceIdx < 0 || voiceIdx >= m_voices.size()) return QString();
    return QFileInfo(m_voices[voiceIdx].sourcePath).fileName();
}

}   // namespace legion
