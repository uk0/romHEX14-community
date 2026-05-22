/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/SimilarFilesDlg.h"
#include "io/winols/BuildIndexProgressDlg.h"
#include "io/winols/SimilarityIndex.h"
#include "io/winols/WinOlsConfig.h"
#include "io/winols/WinOlsCatalogReader.h"
#include "io/winols/WolsTwinFinder.h"
#include "io/winols/WolsCatalogStore.h"
#include "io/winols/WinOlsOpener.h"
#include "io/winols/RomFingerprint.h"
#include "io/ols/OlsImporter.h"

#include "debug/DebugLog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSet>
#include <QTime>
#include <QTimer>
#include <QSettings>
#include <QSlider>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace winols {

// ─── BuildIndexProgressDlg ──────────────────────────────────────────────────
// Moved to its own header (BuildIndexProgressDlg.h) so MainWindow can
// also instantiate it for the explicit "Index WinOLS catalog now" entry.
//
// Old inline version retained below as a no-op compile guard.
#if 0
namespace {
class BuildIndexProgressDlg : public QDialog {
public:
    BuildIndexProgressDlg(SimilarityIndex *idx, const QStringList &roots,
                          QWidget *parent)
        : QDialog(parent), m_idx(idx)
    {
        setWindowTitle(QObject::tr("Building similarity index"));
        setModal(true);
        setMinimumSize(720, 540);
        setStyleSheet(
            "QDialog { background:#0d1117; color:#e6edf3; }"
            "QLabel { color:#c9d1d9; }"
            "QLabel#header  { color:#58a6ff; font-size:14pt; font-weight:600; }"
            "QLabel#caption { color:#8b949e; font-size:9pt; "
            "  text-transform:uppercase; letter-spacing:1px; }"
            "QLabel#big     { color:#e6edf3; font-size:18pt; "
            "  font-family:'Consolas','Courier New',monospace; }"
            "QLabel#tiny    { color:#8b949e; font-size:8pt; }"
            "QPushButton { background:#21262d; color:#e6edf3;"
            "  border:1px solid #30363d; border-radius:4px;"
            "  padding:6px 18px; min-width:80px; }"
            "QPushButton:hover { background:#2d333b; }"
            "QPushButton:disabled { color:#484f58; border-color:#21262d; }"
            "QPushButton#pause { border-color:#d29922; color:#e3b341; }"
            "QPushButton#cancel { border-color:#da3633; color:#f85149; }"
            "QProgressBar {"
            "  background:#161b22; border:1px solid #30363d;"
            "  border-radius:4px; height:18px; text-align:center;"
            "  color:#e6edf3; font-weight:600;"
            "}"
            "QProgressBar::chunk {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "    stop:0 #1f6feb, stop:1 #58a6ff);"
            "  border-radius:3px;"
            "}"
            "QPlainTextEdit { background:#161b22; color:#8b949e;"
            "  border:1px solid #30363d; border-radius:4px;"
            "  font-family:'Consolas','Courier New',monospace; "
            "  font-size:8pt; }");

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 20, 20, 20);
        root->setSpacing(14);

        // ── Header ──
        auto *header = new QLabel(QObject::tr("Indexing ROM fingerprints"));
        header->setObjectName("header");
        root->addWidget(header);

        m_subhead = new QLabel(QObject::tr(
            "Computing similarity fingerprints for every .ols / .kp / .bin "
            "below the configured WinOLS roots.  This is a one-time scan; "
            "subsequent searches will be instant."));
        m_subhead->setWordWrap(true);
        m_subhead->setStyleSheet("color:#8b949e;");
        root->addWidget(m_subhead);

        // ── Big progress bar ──
        m_bar = new QProgressBar();
        m_bar->setRange(0, 100);
        m_bar->setMinimumHeight(22);
        m_bar->setFormat(QStringLiteral("%p%   ·   %v / %m"));
        root->addWidget(m_bar);

        // ── Stats grid (4 columns × 2 rows of caption/value pairs) ──
        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(28);
        grid->setVerticalSpacing(2);

        auto addStat = [&](int col, const QString &cap,
                           QLabel **value) -> void {
            auto *c = new QLabel(cap); c->setObjectName("caption");
            *value  = new QLabel(QStringLiteral("—"));
            (*value)->setObjectName("big");
            grid->addWidget(c,      0, col);
            grid->addWidget(*value, 1, col);
        };
        addStat(0, QObject::tr("Files"),    &m_lblFiles);
        addStat(1, QObject::tr("Rate"),     &m_lblRate);
        addStat(2, QObject::tr("Elapsed"),  &m_lblElapsed);
        addStat(3, QObject::tr("ETA"),      &m_lblEta);
        root->addLayout(grid);

        // ── Current file ──
        auto *curCap = new QLabel(QObject::tr("Currently"));
        curCap->setObjectName("caption");
        root->addWidget(curCap);
        m_lblCurrent = new QLabel(QObject::tr("preparing scan…"));
        m_lblCurrent->setStyleSheet(
            "color:#e6edf3; font-family:Consolas,monospace; font-size:9pt;");
        m_lblCurrent->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_lblCurrent->setWordWrap(true);
        root->addWidget(m_lblCurrent);

        // ── Live log (last ~12 files) ──
        auto *logCap = new QLabel(QObject::tr("Recent activity"));
        logCap->setObjectName("caption");
        root->addWidget(logCap);
        m_log = new QPlainTextEdit();
        m_log->setReadOnly(true);
        m_log->setMaximumBlockCount(200);
        m_log->setMinimumHeight(120);
        root->addWidget(m_log, 1);

        m_lblHint = new QLabel(QObject::tr(
            "Safe to leave running overnight.  Press Pause to suspend, "
            "Cancel to stop early — already-processed files are kept."));
        m_lblHint->setObjectName("tiny");
        m_lblHint->setWordWrap(true);
        root->addWidget(m_lblHint);

        // ── Buttons ──
        auto *btnRow = new QHBoxLayout();
        m_pause  = new QPushButton(QObject::tr("Pause"));
        m_pause->setObjectName("pause");
        m_cancel = new QPushButton(QObject::tr("Cancel"));
        m_cancel->setObjectName("cancel");
        btnRow->addStretch();
        btnRow->addWidget(m_pause);
        btnRow->addWidget(m_cancel);
        root->addLayout(btnRow);

        connect(m_idx, &SimilarityIndex::progress,
                this,  &BuildIndexProgressDlg::onProgress,
                Qt::QueuedConnection);
        connect(m_idx, &SimilarityIndex::rebuildFinished,
                this,  &BuildIndexProgressDlg::onFinished,
                Qt::QueuedConnection);
        connect(m_pause, &QPushButton::clicked, this, [this]() {
            const bool nowPaused = !m_idx->isPaused();
            m_idx->setPaused(nowPaused);
            m_pause->setText(nowPaused ? QObject::tr("Resume")
                                       : QObject::tr("Pause"));
            m_lblHint->setText(nowPaused
                ? QObject::tr("Paused.  No files are being read.  "
                              "Click Resume to continue.")
                : QObject::tr("Safe to leave running overnight.  "
                              "Press Pause to suspend, Cancel to stop."));
        });
        connect(m_cancel, &QPushButton::clicked, this, [this]() {
            m_idx->requestCancel();
            m_cancel->setEnabled(false);
            m_cancel->setText(QObject::tr("Cancelling…"));
            m_lblHint->setText(QObject::tr(
                "Stopping after the current file finishes…"));
        });

        // ── Tick timer keeps Elapsed counter ticking even between
        //    progress() events (which only fire every ~10 files).
        m_tickWall.start();
        m_tick = new QTimer(this);
        m_tick->setInterval(500);
        connect(m_tick, &QTimer::timeout, this, &BuildIndexProgressDlg::tick);
        m_tick->start();

        // Worker
        m_watcher = new QFutureWatcher<void>(this);
        connect(m_watcher, &QFutureWatcher<void>::finished, this, [this]() {
            if (!m_done) accept();
        });
        QFuture<void> fut = QtConcurrent::run([idx, roots]() {
            idx->rebuild(roots);
        });
        m_watcher->setFuture(fut);
    }

private:
    static QString humanDuration(qint64 ms)
    {
        if (ms < 0) ms = 0;
        const qint64 s = ms / 1000;
        const qint64 h = s / 3600;
        const qint64 m = (s % 3600) / 60;
        const qint64 sec = s % 60;
        if (h > 0)
            return QStringLiteral("%1h %2m").arg(h).arg(m, 2, 10, QChar('0'));
        if (m > 0)
            return QStringLiteral("%1m %2s").arg(m).arg(sec, 2, 10, QChar('0'));
        return QStringLiteral("%1s").arg(sec);
    }

    void tick()
    {
        m_lblElapsed->setText(humanDuration(m_tickWall.elapsed()));
    }

    void onProgress(int processed, int total, qint64 totalBytes,
                    qint64 elapsedMs, const QString &currentPath)
    {
        m_lastProcessed = processed;
        m_lastTotal     = total;
        if (total > 0) m_bar->setRange(0, total);
        m_bar->setValue(processed);

        m_lblFiles->setText(QStringLiteral("%1 / %2")
            .arg(processed).arg(total > 0 ? QString::number(total)
                                          : QStringLiteral("?")));

        const double mbps = elapsedMs > 0
            ? (double(totalBytes) / (1024.0 * 1024.0))
              / (double(elapsedMs) / 1000.0)
            : 0.0;
        m_lblRate->setText(QStringLiteral("%1 MB/s").arg(mbps, 0, 'f', 1));
        m_lblElapsed->setText(humanDuration(elapsedMs));

        if (processed > 0 && total > processed) {
            const double pct = double(processed) / double(total);
            const qint64 etaMs = qint64(double(elapsedMs) / pct * (1.0 - pct));
            m_lblEta->setText(humanDuration(etaMs));
        } else if (total > 0 && processed >= total) {
            m_lblEta->setText(QObject::tr("done"));
        }

        if (!currentPath.isEmpty()) {
            const QString fn = QFileInfo(currentPath).fileName();
            m_lblCurrent->setText(fn);
            m_lblCurrent->setToolTip(currentPath);
            // Append to log only when the file changes — avoids
            // hundreds of duplicate lines when a slow file takes
            // multiple progress() ticks.
            if (currentPath != m_lastPath) {
                m_log->appendPlainText(QStringLiteral("%1   %2")
                    .arg(QTime::currentTime().toString("HH:mm:ss"), fn));
                m_lastPath = currentPath;
            }
        }
    }

    void onFinished(int processed, bool cancelled)
    {
        m_done = true;
        m_tick->stop();
        m_subhead->setText(cancelled
            ? QObject::tr("Cancelled by user.  %1 files indexed.")
                  .arg(processed)
            : QObject::tr("Done — %1 files indexed and ready to query.")
                  .arg(processed));
        m_subhead->setStyleSheet(cancelled
            ? "color:#f85149;" : "color:#3fb950;");
        m_lblCurrent->setText(QStringLiteral("—"));
        m_lblHint->setText(QObject::tr("You can close this dialog."));
        m_pause->setEnabled(false);
        m_cancel->setText(QObject::tr("Close"));
        m_cancel->setEnabled(true);
        disconnect(m_cancel, nullptr, nullptr, nullptr);
        connect(m_cancel, &QPushButton::clicked, this, &QDialog::accept);
        // Force the bar to its final state so it doesn't look stuck.
        if (m_lastTotal > 0) m_bar->setValue(m_lastTotal);
    }

    SimilarityIndex *m_idx;
    QFutureWatcher<void> *m_watcher = nullptr;
    QLabel *m_subhead = nullptr, *m_lblFiles = nullptr, *m_lblRate = nullptr;
    QLabel *m_lblElapsed = nullptr, *m_lblEta = nullptr;
    QLabel *m_lblCurrent = nullptr, *m_lblHint = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QProgressBar *m_bar = nullptr;
    QPushButton *m_pause = nullptr, *m_cancel = nullptr;
    QTimer *m_tick = nullptr;
    QElapsedTimer m_tickWall;
    QString m_lastPath;
    int m_lastProcessed = 0, m_lastTotal = 0;
    bool m_done = false;
};

}  // namespace
#endif  // moved to BuildIndexProgressDlg.h

// ─── SimilarFilesDlg ────────────────────────────────────────────────────────

SimilarFilesDlg::SimilarFilesDlg(const QString &sourcePath,
                                 const QByteArray &romBytes,
                                 QWidget *parent)
    : QDialog(parent),
      m_sourcePath(sourcePath),
      m_romBytes(romBytes),
      m_cfg(new Config()),
      m_index(new SimilarityIndex(this))
{
    qCInfo(catFind) << "ctor sourcePath=" << sourcePath
                    << "romBytes=" << romBytes.size() << "B";
    setWindowTitle(tr("Find similar files / projects"));
    setMinimumSize(880, 520);
    buildUi();
    QString err;
    if (!m_index->open(&err)) {
        qCWarning(catFind) << "index open failed:" << err;
        QMessageBox::warning(this, tr("Find similar"),
            tr("Could not open similarity index:\n%1").arg(err));
    } else {
        qCInfo(catFind) << "index opened at" << m_index->dbPath();
    }
    const int rows = m_index->rowCount();
    qCInfo(catFind) << "index rowCount=" << rows;

    // Tryb A — exact + similar WinOLS matches from the local extract DB
    // (independent of WinOLS's format).  If the extract is empty, offer a
    // one-time sync first.  Deferred so the prompt appears after the dialog.
    QTimer::singleShot(0, this, &SimilarFilesDlg::maybeSyncThenFind);

    m_status->setText(tr("Index: %1 files cached").arg(rows));
    if (rows == 0) {
        // Do NOT auto-launch the (multi-hour) fingerprint rebuild — Tryb A
        // already gives exact twins from the catalog.  Offer the rebuild
        // only as an explicit choice for fuzzy similarity.
        qCInfo(catFind) << "empty index — exact-twin mode only";
        m_status->setText(tr("Fuzzy index empty — showing exact WinOLS twins. "
                             "Click “Rebuild index” for fuzzy similarity."));
    } else {
        runQuery();
    }
}

SimilarFilesDlg::~SimilarFilesDlg()
{
    qCInfo(catFind) << "dtor — chosen=" << m_chosen;
    delete m_needle;
    delete m_cfg;
}

void SimilarFilesDlg::buildUi()
{
    setStyleSheet(
        "QDialog { background:#1c2128; color:#e6edf3; }"
        "QTreeWidget { background:#0d1117; color:#e6edf3; "
        "  alternate-background-color:#111820; border:1px solid #30363d; "
        "  selection-background-color:#1f6feb; }"
        "QHeaderView::section { background:#161b22; color:#8b949e; "
        "  border:none; border-right:1px solid #30363d; "
        "  border-bottom:1px solid #30363d; padding:4px 8px; }"
        "QPushButton { background:#21262d; color:#e6edf3; "
        "  border:1px solid #30363d; border-radius:4px; padding:5px 14px; }"
        "QPushButton:hover { background:#2d333b; }"
        "QPushButton#openBtn { border-color:#238636; color:#3fb950; }"
        "QLabel { color:#8b949e; }"
        "QSlider::groove:horizontal { height:6px; background:#30363d; "
        "  border-radius:3px; }"
        "QSlider::handle:horizontal { background:#58a6ff; width:12px; "
        "  margin:-4px 0; border-radius:6px; }");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *srcRow = new QHBoxLayout();
    auto *srcLbl = new QLabel(tr("Source:"));
    auto *srcVal = new QLabel(QFileInfo(m_sourcePath).fileName().isEmpty()
        ? tr("<active project>") : m_sourcePath);
    srcVal->setStyleSheet("color:#e6edf3;");
    srcVal->setToolTip(m_sourcePath);
    srcRow->addWidget(srcLbl);
    srcRow->addWidget(srcVal, 1);
    root->addLayout(srcRow);

    auto *minRow = new QHBoxLayout();
    auto *minTxt = new QLabel(tr("Min similarity:"));
    m_minSlide = new QSlider(Qt::Horizontal);
    m_minSlide->setRange(5, 99);
    m_minSlide->setValue(QSettings()
        .value(QStringLiteral("similarFiles/minPct"), 30).toInt());
    m_minLabel = new QLabel(QString::number(m_minSlide->value()) + "%");
    m_minLabel->setMinimumWidth(40);
    minRow->addWidget(minTxt);
    minRow->addWidget(m_minSlide, 1);
    minRow->addWidget(m_minLabel);
    root->addLayout(minRow);

    m_tree = new QTreeWidget();
    m_tree->setColumnCount(8);
    m_tree->setHeaderLabels({
        tr("% Match"), tr("Byte%"), tr("Source"),
        tr("Make"), tr("Model"), tr("ECU"), tr("SW"), tr("File")});
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setStretchLastSection(true);
    root->addWidget(m_tree, 1);

    m_busyBar = new QProgressBar();
    m_busyBar->setRange(0, 0);
    m_busyBar->setMaximumHeight(3);
    m_busyBar->setTextVisible(false);
    m_busyBar->hide();
    root->addWidget(m_busyBar);

    m_status = new QLabel();
    root->addWidget(m_status);

    auto *btnRow = new QHBoxLayout();
    m_syncBtn = new QPushButton(tr("Sync WinOLS"));
    m_syncBtn->setToolTip(tr("Copy the WinOLS catalog into a local database "
                             "(read-only, one-time; survives WinOLS format changes)"));
    m_rebuild = new QPushButton(tr("Rebuild index"));
    m_openBtn = new QPushButton(tr("Open"));
    m_openBtn->setObjectName("openBtn");
    m_openBtn->setEnabled(false);
    m_cmpBtn  = new QPushButton(tr("Open as comparison"));
    m_cmpBtn->setEnabled(false);
    auto *closeBtn = new QPushButton(tr("Close"));
    btnRow->addWidget(m_syncBtn);
    btnRow->addWidget(m_rebuild);
    btnRow->addStretch();
    btnRow->addWidget(m_openBtn);
    btnRow->addWidget(m_cmpBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    connect(m_minSlide, &QSlider::valueChanged,
            this, &SimilarFilesDlg::onMinChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &SimilarFilesDlg::onRowActivated);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        const bool any = !m_tree->selectedItems().isEmpty();
        m_openBtn->setEnabled(any);
        m_cmpBtn->setEnabled(any);
    });
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        const auto sel = m_tree->selectedItems();
        if (!sel.isEmpty()) onRowActivated(sel.first(), 0);
    });
    connect(m_cmpBtn, &QPushButton::clicked, this, [this]() {
        const auto sel = m_tree->selectedItems();
        if (sel.isEmpty()) return;
        const QString p = resolveItemPath(sel.first());
        if (!p.isEmpty()) {
            emit compareWithRequested(p);
            accept();
        }
    });
    connect(m_rebuild, &QPushButton::clicked,
            this, &SimilarFilesDlg::onRebuildIndex);
    connect(m_syncBtn, &QPushButton::clicked,
            this, &SimilarFilesDlg::syncWolsCatalog);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void SimilarFilesDlg::runQuery()
{
    qCInfo(catFind) << "runQuery start";
    if (!m_index || !m_index->isOpen()) {
        qCWarning(catFind) << "runQuery aborted: index not open";
        return;
    }
    if (!m_needle) {
        try {
            m_needle = new RomFingerprint(fingerprint(QByteArrayView(m_romBytes)));
            qCInfo(catFind) << "needle fingerprint computed; empty="
                            << m_needle->isEmpty();
        } catch (const std::exception &e) {
            qCCritical(catFind) << "fingerprint() threw:" << e.what();
            m_status->setText(tr("Could not fingerprint source ROM: %1")
                                  .arg(QString::fromUtf8(e.what())));
            return;
        } catch (...) {
            qCCritical(catFind) << "fingerprint() threw unknown exception";
            m_status->setText(tr("Could not fingerprint source ROM"));
            return;
        }
    }
    if (m_needle->isEmpty()) {
        qCWarning(catFind) << "needle empty — aborting query";
        m_status->setText(tr("Source ROM is empty or all-padding — nothing to compare."));
        return;
    }
    if (m_queryRunning.load()) {
        qCInfo(catFind) << "runQuery skipped: already running";
        return;
    }
    m_queryRunning.store(true);
    m_busyBar->show();
    m_minSlide->setEnabled(false);
    m_status->setText(tr("Searching %1 files…").arg(m_index->rowCount()));

    const RomFingerprint needle = *m_needle;
    const int minPct = m_minSlide->value();
    SimilarityIndex *idx = m_index;
    qCInfo(catFind) << "dispatching worker query: minPct=" << minPct
                    << " indexRows=" << idx->rowCount();

    auto *watcher = new QFutureWatcher<QPair<QVector<SimilarityMatch>, qint64>>(this);
    connect(watcher, &QFutureWatcher<QPair<QVector<SimilarityMatch>, qint64>>::finished,
            this, [this, watcher]() {
        try {
            auto result = watcher->result();
            qCInfo(catFind) << "worker finished: matches="
                            << result.first.size()
                            << " ms=" << result.second;
            onQueryFinished(result.first, result.second);
        } catch (const std::exception &e) {
            qCCritical(catFind) << "worker threw:" << e.what();
            m_queryRunning.store(false);
            m_busyBar->hide();
            m_minSlide->setEnabled(true);
            m_status->setText(tr("Search failed: %1")
                                  .arg(QString::fromUtf8(e.what())));
        } catch (...) {
            qCCritical(catFind) << "worker threw unknown exception";
            m_queryRunning.store(false);
            m_busyBar->hide();
            m_minSlide->setEnabled(true);
            m_status->setText(tr("Search failed"));
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([idx, needle, minPct]() {
        QElapsedTimer t; t.start();
        auto matches = idx->findSimilar(needle, minPct, 5000);
        return qMakePair(matches, t.elapsed());
    }));
}

void SimilarFilesDlg::onQueryFinished(const QVector<SimilarityMatch> &matches,
                                       qint64 ms)
{
    qCInfo(catFind) << "onQueryFinished: matches=" << matches.size()
                    << " ms=" << ms;
    m_queryRunning.store(false);
    m_busyBar->hide();
    m_minSlide->setEnabled(true);
    populateTable(matches);
    m_status->setText(tr("%1 matches · %2 ms · index size %3")
                          .arg(matches.size())
                          .arg(ms)
                          .arg(m_index->rowCount()));
    try {
        enrichFromCatalog();
    } catch (const std::exception &e) {
        qCCritical(catFind) << "enrichFromCatalog threw:" << e.what();
    } catch (...) {
        qCCritical(catFind) << "enrichFromCatalog threw unknown exception";
    }
    qCInfo(catFind) << "onQueryFinished done";
}

namespace {
// Tree item that sorts numerically by a match score stored in UserRole+10
// (descending in the view), instead of by the fragile "% string".  Used for
// exact twins, fuzzy candidates and MinHash rows alike so they interleave
// in a sensible order regardless of which column the user sorts by.
class MatchItem : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem &o) const override {
        return data(0, Qt::UserRole + 10).toDouble()
             < o.data(0, Qt::UserRole + 10).toDouble();
    }
};
}  // namespace

void SimilarFilesDlg::populateTable(const QVector<SimilarityMatch> &matches)
{
    qCInfo(catFind) << "populateTable: " << matches.size() << " matches";
    m_tree->setSortingEnabled(false);

    // Keep the exact-twin rows (Tryb A) on top; drop only the previous
    // fuzzy rows.  Exact rows are tagged with UserRole+5 == true.
    QSet<QString> exactPaths;
    for (int i = m_tree->topLevelItemCount() - 1; i >= 0; --i) {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (it->data(0, Qt::UserRole + 5).toBool()) {
            const QString p = it->data(0, Qt::UserRole).toString();
            if (!p.isEmpty()) exactPaths.insert(p);
        } else {
            delete m_tree->takeTopLevelItem(i);
        }
    }

    for (const auto &m : matches) {
        if (!m.path.isEmpty() && exactPaths.contains(m.path))
            continue;   // already shown as an exact WinOLS twin
        // Headline % is the best of Jaccard / containment, so a small ROM
        // fully inside a larger dump still scores high.  Append the .ols
        // version label that actually matched, when present.
        const QString fname = QFileInfo(m.path).fileName()
            + (m.versionLabel.isEmpty() ? QString()
                  : QStringLiteral(" [%1]").arg(m.versionLabel));
        auto *it = new MatchItem(QStringList{
            QString("%1%").arg(m.score.bestPct(), 3),
            QStringLiteral("…"),                      // byte-match placeholder
            tr("(file)"),
            QString(), QString(), QString(), QString(),
            fname
        });
        it->setData(0, Qt::UserRole, m.path);
        it->setData(0, Qt::UserRole + 1, double(m.score.best()));
        it->setData(0, Qt::UserRole + 10, double(m.score.bestPct()));
        it->setToolTip(0, tr("Jaccard %1%  ·  containment %2%")
                              .arg(m.score.wholePct()).arg(m.score.containPct()));
        it->setToolTip(7, m.path);
        m_tree->addTopLevelItem(it);
    }
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::DescendingOrder);

    // Kick off the byte-match scoring on a worker thread for the top
    // N rows by MinHash score.  This is the only honest way to tell
    // a real byte-twin from an n-gram lookalike.
    scheduleByteMatchScan(matches);
}

// ── Tryb A: exact WinOLS twins straight from the catalog ────────────────
//
// Computes the needle's CRC32 region identity and matches it against the
// `colx4` fingerprints stored in every Cache_*.db.  Deterministic, needs
// no MinHash index, and works even when that index is empty/broken.

void SimilarFilesDlg::maybeSyncThenFind()
{
    if (m_romBytes.isEmpty()) return;
    bool populated = false;
    {
        WolsCatalogStore store;
        QString e;
        if (store.open(&e)) populated = store.isPopulated();
    }
    if (populated) { findExactTwins(); return; }

    qCInfo(catFind) << "WinOLS extract empty — offering one-time sync";
    const auto r = QMessageBox::question(this, tr("Find similar"),
        tr("The WinOLS catalog hasn't been copied locally yet.\n\n"
           "Copy it now? This reads your WinOLS Cache_*.db once (read-only) into "
           "a local database, so similarity search works without touching WinOLS "
           "again — and keeps working even if WinOLS later changes its format."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (r == QMessageBox::Yes)
        syncWolsCatalog();
    else
        m_status->setText(tr("WinOLS catalog not copied locally — "
                             "click “Sync WinOLS” to enable similarity search."));
}

void SimilarFilesDlg::syncWolsCatalog()
{
    if (m_syncRunning.exchange(true)) return;
    qCInfo(catFind) << "syncWolsCatalog start";

    auto *prog = new QProgressDialog(
        tr("Copying WinOLS catalog locally (one-time)…"),
        QString() /* no cancel button */, 0, 0, this);
    prog->setWindowTitle(tr("Sync WinOLS catalog"));
    prog->setWindowModality(Qt::WindowModal);
    prog->setMinimumDuration(0);
    prog->setAutoClose(false);
    prog->setAutoReset(false);
    prog->setValue(0);
    prog->show();

    QPointer<QProgressDialog> pp = prog;
    auto *watcher = new QFutureWatcher<SyncStats>(this);
    connect(watcher, &QFutureWatcher<SyncStats>::finished, this, [this, watcher, pp]() {
        m_syncRunning.store(false);
        SyncStats st;
        try { st = watcher->result(); } catch (...) {}
        if (pp) { pp->close(); pp->deleteLater(); }
        m_status->setText(tr("WinOLS catalog copied: %1 files · %2 source DBs "
                             "(%3 unreadable) · %4 ms")
                              .arg(st.rowsTotal)
                              .arg(st.dbsScanned + st.dbsSkipped)
                              .arg(st.dbsFailed)
                              .arg(st.elapsedMs));
        findExactTwins();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([pp]() {
        WolsTwinFinder f;
        return f.sync([pp](int done, int total, const QString &cur) {
            QMetaObject::invokeMethod(qApp, [pp, done, total, cur]() {
                if (!pp) return;
                pp->setMaximum(total);
                pp->setValue(done);
                if (!cur.isEmpty())
                    pp->setLabelText(QObject::tr("Copying WinOLS catalog: %1").arg(cur));
            }, Qt::QueuedConnection);
        });
    }));
}

namespace { struct ExactFindResult { QVector<ExactTwin> twins; qint64 rs = -1, re = -1; }; }

void SimilarFilesDlg::findExactTwins()
{
    if (m_romBytes.isEmpty()) return;
    if (m_exactRunning.exchange(true)) return;
    qCInfo(catFind) << "findExactTwins start; romBytes=" << m_romBytes.size();

    const QByteArray bytes = m_romBytes;
    auto *watcher = new QFutureWatcher<ExactFindResult>(this);
    connect(watcher, &QFutureWatcher<ExactFindResult>::finished,
            this, [this, watcher]() {
        m_exactRunning.store(false);
        try {
            const ExactFindResult fr = watcher->result();
            m_dataStart = fr.rs;
            m_dataEnd   = fr.re;
            populateExactTwins(fr.twins);
            computeSimilarPercents(fr.twins);
        } catch (const std::exception &e) {
            qCCritical(catFind) << "findExactTwins worker threw:" << e.what();
        } catch (...) {
            qCCritical(catFind) << "findExactTwins worker threw unknown";
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([bytes]() {
        ExactFindResult fr;
        WolsTwinFinder finder;
        QPair<qint64, qint64> region(-1, -1);
        fr.twins = finder.find(bytes, &region);
        fr.rs = region.first;
        fr.re = region.second;
        return fr;
    }));
}

void SimilarFilesDlg::populateExactTwins(const QVector<ExactTwin> &twins)
{
    if (!m_tree) return;
    qCInfo(catFind) << "populateExactTwins:" << twins.size() << "twins";

    m_tree->setSortingEnabled(false);

    // Idempotent: drop any previous exact-twin rows (UserRole+5 flag),
    // leave fuzzy rows untouched.
    for (int i = m_tree->topLevelItemCount() - 1; i >= 0; --i) {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (it->data(0, Qt::UserRole + 5).toBool())
            delete m_tree->takeTopLevelItem(i);
    }

    const QString srcAbs = m_sourcePath.isEmpty()
        ? QString() : QFileInfo(m_sourcePath).absoluteFilePath();
    const QColor green("#3fb950");
    const QColor amber("#d29922");
    int exact = 0, similar = 0;
    for (const ExactTwin &t : twins) {
        // Never list the source file itself.
        if (!t.path.isEmpty() && !srcAbs.isEmpty() &&
            QFileInfo(t.path).absoluteFilePath().compare(
                srcAbs, Qt::CaseInsensitive) == 0)
            continue;

        MatchItem *it;
        if (t.similar) {
            // Fuzzy candidate: project not matched here; the data-area %
            // is computed for real afterwards (computeSimilarPercents).
            // Until then show a provisional estimate from the fingerprint.
            const int est = qBound(0, int(100.0 * (1.0 - t.dataDist) + 0.5), 100);
            it = new MatchItem(QStringList{
                QStringLiteral("-"),
                QStringLiteral("~%1%").arg(est),
                tr("WinOLS similar (data area)"),
                t.make, t.model, t.ecuModel, t.swNumber, t.filename});
            for (int c = 0; c < 3; ++c) it->setForeground(c, amber);
            it->setData(0, Qt::UserRole + 9, true);          // is-similar
            it->setData(0, Qt::UserRole + 10, double(est));  // sort key (refined)
            ++similar;
        } else {
            QString tag;
            if (t.projectTwin && t.dataTwin) tag = tr("WinOLS twin (project + data)");
            else if (t.projectTwin)          tag = tr("WinOLS twin (project)");
            else                             tag = tr("WinOLS twin (data area)");
            it = new MatchItem(QStringList{
                t.projectTwin ? QStringLiteral("100%") : QStringLiteral("-"),
                t.dataTwin    ? QStringLiteral("100%") : QStringLiteral("-"),
                tag, t.make, t.model, t.ecuModel, t.swNumber, t.filename});
            for (int c = 0; c < 3; ++c) it->setForeground(c, green);
            it->setData(0, Qt::UserRole + 10, t.projectTwin ? 100.5 : 100.0);
            ++exact;
        }
        it->setData(0, Qt::UserRole, t.path);          // resolved path (may be empty)
        it->setData(0, Qt::UserRole + 5, true);        // catalog-row marker (lazy open)
        it->setData(0, Qt::UserRole + 6, t.dbBasename);
        it->setData(0, Qt::UserRole + 7, t.filename);
        it->setToolTip(7, t.path.isEmpty()
            ? tr("%1 (catalog: %2) — double-click to locate & open")
                  .arg(t.filename, t.dbBasename)
            : t.path);
        m_tree->addTopLevelItem(it);
    }

    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::DescendingOrder);

    if (exact + similar > 0) {
        m_status->setText(tr("%1 exact + %2 similar WinOLS match(es) — "
                             "computing exact %…").arg(exact).arg(similar));
    }
    qCInfo(catFind) << "populateExactTwins done; exact=" << exact
                    << "similar=" << similar;
}

namespace {

// Fraction (0..1) of matching bytes of @p pat against @p hay at offset @p off,
// sampling every @p step bytes.  -1 if the window doesn't fit.
double scorePatternAt(const QByteArray &pat, const QByteArray &hay,
                      int off, int step)
{
    const int sz = pat.size();
    if (off < 0 || off + sz > hay.size()) return -1.0;
    const auto *pp = reinterpret_cast<const uint8_t *>(pat.constData());
    const auto *ph = reinterpret_cast<const uint8_t *>(hay.constData());
    int n = 0, m = 0;
    for (int i = 0; i < sz; i += step) { if (pp[i] == ph[off + i]) ++m; ++n; }
    return n ? double(m) / n : -1.0;
}

// Matching fraction of the needle data region @p region against @p hay,
// ADDRESS-ALIGNED like WinOLS's data-area %: the region sits at the same
// offset in a same-layout ROM; in a partial file that *is* the data region
// it sits at offset 0.  No free slide — sliding finds spurious high matches
// where the calibration region happens to line up over common code/padding
// (that produced bogus 98% where the true data-area match was 66%).
double bestRegionMatch(const QByteArray &region, const QByteArray &hay,
                       qint64 preferred)
{
    if (region.isEmpty() || hay.isEmpty()) return 0.0;
    double s = -1.0;
    if (preferred >= 0 && preferred + region.size() <= hay.size())
        s = scorePatternAt(region, hay, int(preferred), 1);   // same layout
    else if (region.size() <= hay.size())
        s = scorePatternAt(region, hay, 0, 1);                // partial file
    return s < 0.0 ? 0.0 : s;
}

}  // namespace

void SimilarFilesDlg::computeSimilarPercents(const QVector<ExactTwin> &twins)
{
    if (m_simPctRunning.exchange(true)) return;

    struct Cand { QString db, fn; };
    QVector<Cand> cands;
    for (const ExactTwin &t : twins)
        if (t.similar) cands.append({t.dbBasename, t.filename});

    if (cands.isEmpty() || m_dataStart < 0 || m_dataEnd < m_dataStart ||
        m_dataStart >= m_romBytes.size()) {
        m_simPctRunning.store(false);
        return;
    }
    const qint64 len = qMin<qint64>(m_dataEnd - m_dataStart + 1,
                                    m_romBytes.size() - m_dataStart);
    const QByteArray region = m_romBytes.mid(int(m_dataStart), int(len));
    const qint64 preferred  = m_dataStart;
    qCInfo(catFind) << "computeSimilarPercents:" << cands.size()
                    << "candidates; region=" << region.size() << "B";

    Config cfg;
    const QStringList roots          = cfg.scanFallback();
    const QHash<QString, QString> fr = cfg.fileRoots();
    QPointer<SimilarFilesDlg> self = this;

    (void)QtConcurrent::run([self, cands, region, roots, fr, preferred]() {
        // 1. Resolve filenames -> paths: fileRoots fast path, then ONE
        //    recursive walk over scanFallback (early-exit when all found).
        QHash<QString, QString> pathByKey;          // "db|fn" -> path
        QHash<QString, QVector<int>> needed;        // lower(fn) -> cand idx
        for (int i = 0; i < cands.size(); ++i) {
            const Cand &c = cands[i];
            auto it = fr.constFind(c.db);
            if (it != fr.constEnd()) {
                const QString cand = QDir(it.value()).filePath(c.fn);
                if (QFileInfo::exists(cand)) {
                    pathByKey.insert(c.db + QLatin1Char('|') + c.fn, cand);
                    continue;
                }
            }
            needed[c.fn.toLower()].append(i);
        }
        for (const QString &root : roots) {
            if (needed.isEmpty()) break;
            QDirIterator scan(root, QDir::Files, QDirIterator::Subdirectories);
            while (scan.hasNext()) {
                scan.next();
                auto it = needed.find(scan.fileName().toLower());
                if (it != needed.end()) {
                    for (int idx : it.value())
                        pathByKey.insert(cands[idx].db + QLatin1Char('|')
                                         + cands[idx].fn, scan.filePath());
                    needed.erase(it);
                    if (needed.isEmpty()) break;
                }
            }
        }

        // 2. Read + extract + compare each candidate -> emit true %.
        for (const Cand &c : cands) {
            if (!self) return;
            const QString path = pathByKey.value(c.db + QLatin1Char('|') + c.fn);
            double pct = -1.0;
            if (!path.isEmpty()) {
                QFile f(path);
                if (f.open(QIODevice::ReadOnly)) {
                    const QByteArray raw = f.readAll();
                    f.close();
                    QVector<QByteArray> streams;
                    streams.append(raw);
                    try {
                        ols::OlsImportResult r =
                            ols::OlsImporter::importFromBytes(raw);
                        if (r.error.isEmpty())
                            for (const auto &v : r.versions)
                                if (!v.romData.isEmpty()) streams.append(v.romData);
                    } catch (...) {}
                    double best = 0.0;
                    for (const QByteArray &s : streams)
                        best = qMax(best, bestRegionMatch(region, s, preferred));
                    pct = best * 100.0;
                }
            }
            const QString cdb = c.db, cfn = c.fn;
            QMetaObject::invokeMethod(self.data(),
                [self, cdb, cfn, pct, path]() {
                    if (self) self->onSimilarPctResult(cdb, cfn, pct, path);
                }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(self.data(), [self]() {
            if (!self) return;
            self->m_simPctRunning.store(false);
            qCInfo(catFind) << "computeSimilarPercents done";
        }, Qt::QueuedConnection);
    });
}

void SimilarFilesDlg::onSimilarPctResult(const QString &dbBasename,
                                         const QString &filename,
                                         double dataPct, const QString &path)
{
    if (!m_tree) return;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (!it->data(0, Qt::UserRole + 9).toBool()) continue;   // similar only
        if (it->data(0, Qt::UserRole + 6).toString() != dbBasename) continue;
        if (it->data(0, Qt::UserRole + 7).toString() != filename) continue;

        if (!path.isEmpty() && it->data(0, Qt::UserRole).toString().isEmpty()) {
            it->setData(0, Qt::UserRole, path);
            it->setToolTip(7, path);
        }
        if (dataPct >= 0.0) {
            it->setText(1, QStringLiteral("%1%").arg(dataPct, 0, 'f', 0));
            it->setData(0, Qt::UserRole + 10, dataPct);
            it->setForeground(1, dataPct >= 95.0 ? QColor("#3fb950")
                               : dataPct >= 70.0 ? QColor("#d29922")
                                                 : QColor("#f0883e"));
        } else {
            it->setText(1, tr("n/a"));   // file not on disk / unreadable
        }
        break;
    }
}

namespace {

// Process-wide WOLS catalog cache.  Built lazily on first Find-Similar-Files
// open (parallel SQLite reads of every Cache_*.db) and reused thereafter so
// repeated dialog opens within one rx14 session are instant.  Stale-after-edit
// is acceptable because the user can always restart rx14 (or hit "Rebuild
// index", which the dialog also exposes for the similarity index proper).
QMutex                          sCatalogMutex;
QHash<QString, CatalogRecord>   sCatalogCache;
std::atomic<bool>               sCatalogLoaded{false};

QStringList parseVersionNames(const QString &versionsInfo)
{
    if (versionsInfo.isEmpty()) return {};
    int parenOpen = versionsInfo.indexOf(QLatin1Char('('));
    int parenClose = versionsInfo.lastIndexOf(QLatin1Char(')'));
    if (parenOpen < 0 || parenClose <= parenOpen) return {};

    QString inner = versionsInfo.mid(parenOpen + 1, parenClose - parenOpen - 1);
    QStringList entries = inner.split(QStringLiteral(",\t"), Qt::SkipEmptyParts);
    if (entries.isEmpty())
        entries = inner.split(QLatin1Char(','), Qt::SkipEmptyParts);

    QStringList names;
    names.reserve(entries.size());
    for (const QString &e : entries) {
        QString s = e.trimmed();
        int colon = s.indexOf(QStringLiteral(":\t"));
        if (colon < 0) colon = s.indexOf(QLatin1Char(':'));
        if (colon > 0) s = s.left(colon).trimmed();
        if (!s.isEmpty()) names.append(s);
    }
    return names;
}

}  // anon namespace

void SimilarFilesDlg::enrichFromCatalog()
{
    if (!m_cfg) {
        qCWarning(catFind) << "enrichFromCatalog aborted: no Config";
        return;
    }
    // Fast path: process-wide cache already populated from an earlier dialog
    // instance.  No SQLite reads, no worker dispatch — just paint.
    if (sCatalogLoaded.load(std::memory_order_acquire)) {
        qCInfo(catFind) << "enrichFromCatalog: using process-wide cache ("
                        << sCatalogCache.size() << "entries)";
        applyCatalogToRows();
        return;
    }
    if (m_catalogLoading.load()) {
        qCInfo(catFind) << "enrichFromCatalog: load already in flight";
        return;
    }
    qCInfo(catFind) << "enrichFromCatalog start (async)";
    m_catalogLoading.store(true);

    Config *cfg = m_cfg;
    auto *watcher =
        new QFutureWatcher<QHash<QString, CatalogRecord>>(this);
    connect(watcher, &QFutureWatcher<QHash<QString, CatalogRecord>>::finished,
            this, [this, watcher]() {
        m_catalogLoading.store(false);
        try {
            auto map = watcher->result();
            qCInfo(catFind) << "catalog worker finished:" << map.size()
                            << "entries";
            // Publish to the process-wide cache so the next dialog opens
            // instantly.
            {
                QMutexLocker lock(&sCatalogMutex);
                sCatalogCache = std::move(map);
                sCatalogLoaded.store(true, std::memory_order_release);
            }
            applyCatalogToRows();
        } catch (const std::exception &e) {
            qCCritical(catFind) << "catalog worker threw:" << e.what();
        } catch (...) {
            qCCritical(catFind) << "catalog worker threw unknown exception";
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([cfg]() {
        QElapsedTimer t; t.start();
        const auto dbs = cfg->discoverCacheDbs();
        qCInfo(catFind) << "[worker] discovered" << dbs.size() << "Cache_*.db";
        // Read DBs in parallel — each WinOlsCatalogReader uses a UUID-named
        // SQLite connection so concurrent reads don't conflict.  29 DBs
        // sequentially = ~7s; parallel across 8 cores = ~1-2s.
        QVector<QVector<CatalogRecord>> perDb =
            QtConcurrent::blockingMapped(dbs, [](const QString &dbPath) {
                return WinOlsCatalogReader::dumpAll(dbPath);
            });
        qCInfo(catFind) << "[worker] parallel reads done in" << t.elapsed() << "ms";
        // Merge serially on this thread (QHash is not thread-safe).
        QHash<QString, CatalogRecord> byName;
        int totalRecs = 0;
        for (const auto &recs : perDb) totalRecs += recs.size();
        byName.reserve(totalRecs);
        for (const auto &recs : perDb) {
            for (const auto &r : recs) {
                if (!r.filename.isEmpty())
                    byName.insert(r.filename, r);
            }
        }
        qCInfo(catFind) << "[worker] catalog map:" << byName.size()
                        << "entries; total" << t.elapsed() << "ms";
        return byName;
    }));
}

void SimilarFilesDlg::applyCatalogToRows()
{
    if (!sCatalogLoaded.load(std::memory_order_acquire)) return;
    QElapsedTimer t; t.start();
    const int N = m_tree->topLevelItemCount();
    qCInfo(catFind) << "applyCatalogToRows: enriching" << N << "rows";

    // Batching: disable sorting + updates while mutating, otherwise every
    // setText triggers a re-sort + repaint.
    const bool wasSorting = m_tree->isSortingEnabled();
    m_tree->setSortingEnabled(false);
    m_tree->setUpdatesEnabled(false);

    QMutexLocker lock(&sCatalogMutex);
    const auto &byName = sCatalogCache;
    int matched = 0;
    for (int i = 0; i < N; ++i) {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (it->data(0, Qt::UserRole + 5).toBool())
            continue;   // exact WinOLS twin — already carries its own metadata
        // Columns: 0 %Match | 1 Byte% | 2 Source | 3 Make | 4 Model |
        //          5 ECU | 6 SW | 7 File.  The filename lives in col 7.
        const QString fn = it->text(7);
        auto found = byName.constFind(fn);
        if (found == byName.constEnd()) continue;

        ++matched;
        const QString path = it->data(0, Qt::UserRole).toString();
        const auto &r = found.value();
        it->setText(2, QFileInfo(path).dir().dirName());   // Source = folder
        it->setText(3, r.make);
        it->setText(4, r.model);
        it->setText(5, r.ecuModel);
        it->setText(6, r.swNumber);

        // Upgrade the File-column tooltip with version names when known.
        // Setting tooltips on every column of every row was the prior
        // bottleneck; hovering the File column is the natural interaction.
        const QStringList vers = parseVersionNames(r.versionsInfo);
        if (!vers.isEmpty()) {
            it->setToolTip(7, path + QStringLiteral("\nVersions: ")
                              + vers.join(QStringLiteral(", ")));
        }
    }

    m_tree->setUpdatesEnabled(true);
    m_tree->setSortingEnabled(wasSorting);
    qCInfo(catFind) << "applyCatalogToRows done in" << t.elapsed()
                    << "ms (" << matched << "matched of" << N << ")";
}

void SimilarFilesDlg::onMinChanged(int v)
{
    m_minLabel->setText(QString::number(v) + "%");
    QSettings().setValue(QStringLiteral("similarFiles/minPct"), v);
    if (m_queryRunning.load()) return;
    runQuery();
}

QString SimilarFilesDlg::resolveItemPath(QTreeWidgetItem *it)
{
    if (!it) return {};
    QString p = it->data(0, Qt::UserRole).toString();
    if (!p.isEmpty()) return p;
    if (!it->data(0, Qt::UserRole + 5).toBool()) return {};   // not an exact twin

    // Exact WinOLS twin not yet resolved — locate it on disk now (the
    // recursive scanFallback search is deferred to this point).
    const QString db = it->data(0, Qt::UserRole + 6).toString();
    const QString fn = it->data(0, Qt::UserRole + 7).toString();
    qCInfo(catFind) << "lazy-resolving twin" << fn << "from" << db;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    Config cfg;
    QString err;
    p = Opener::resolve(cfg, db, fn, &err);
    QApplication::restoreOverrideCursor();
    if (p.isEmpty()) {
        QMessageBox::information(this, tr("Find similar"),
            tr("Could not locate the file on disk:\n%1\n\n%2").arg(fn, err));
        return {};
    }
    it->setData(0, Qt::UserRole, p);   // cache for next time
    it->setToolTip(7, p);
    return p;
}

void SimilarFilesDlg::onRowActivated(QTreeWidgetItem *it, int)
{
    qCInfo(catFind) << "onRowActivated";
    if (!it) {
        qCWarning(catFind) << "onRowActivated: null item";
        return;
    }
    const QString p = resolveItemPath(it);
    qCInfo(catFind) << "  selected path=" << p;
    if (p.isEmpty()) {
        qCWarning(catFind) << "onRowActivated: empty/unresolved path on item";
        return;
    }
    m_chosen = p;
    qCInfo(catFind) << "emitting openSimilarRequested + accept()";
    emit openSimilarRequested(p);
    accept();
}

void SimilarFilesDlg::onRebuildIndex()
{
    qCInfo(catFind) << "onRebuildIndex start";
    if (!m_cfg) m_cfg = new Config();
    QStringList roots = m_cfg->scanFallback();
    for (const QString &r : m_cfg->dbRoots()) roots << r;
    qCInfo(catFind) << "rebuild roots count=" << roots.size();
    if (roots.isEmpty()) {
        qCWarning(catFind) << "onRebuildIndex: no roots configured";
        QMessageBox::warning(this, tr("Find similar"),
            tr("No scan roots configured.  Open Project Manager → "
               "WOLS Catalog → Settings to import paths from "
               "ols.cfg first."));
        return;
    }
    BuildIndexProgressDlg dlg(m_index, roots, this);
    dlg.exec();
    qCInfo(catFind) << "rebuild dialog closed; running query";
    runQuery();
}

// ── byte-match scan ────────────────────────────────────────────────────
//
// The MinHash fingerprint in the index measures n-gram overlap — two ROMs
// can share 98% of their n-grams while only ~60% of their bytes line up.
// To let the user pick a real byte-twin (not just a code-pattern lookalike)
// we run a real byte-by-byte comparison against the user's source ROM for
// the top-N matches and surface the result in a second percentage column.

void SimilarFilesDlg::scheduleByteMatchScan(const QVector<SimilarityMatch> &matches)
{
    static constexpr int kTopN = 30;
    if (m_romBytes.isEmpty()) return;
    const QByteArray needle = m_romBytes;
    const int n = qMin(kTopN, matches.size());

    for (int i = 0; i < n; ++i) {
        const QString path = matches[i].path;
        if (path.isEmpty()) continue;
        if (m_byteMatchCache.contains(path)) {
            onByteMatchResult(path, m_byteMatchCache.value(path));
            continue;
        }
        QPointer<SimilarFilesDlg> self = this;
        QtConcurrent::run([self, path, needle]() {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) return;
            const QByteArray raw = f.readAll();
            f.close();
            if (raw.isEmpty()) return;

            // Gather every plausible byte stream the file might contain:
            //   - raw file (covers .bin / .ori / unknown formats)
            //   - every Version_N.romData from OlsImporter (covers .ols/.kp)
            // Then score each one and keep the maximum.  This handles the
            // common case where the user's ROM corresponds to a later
            // tuned version of a multi-version .ols rather than v0.
            QVector<QByteArray> candidates;
            candidates.append(raw);
            try {
                ols::OlsImportResult r = ols::OlsImporter::importFromBytes(raw);
                if (r.error.isEmpty()) {
                    for (const auto &v : r.versions) {
                        if (!v.romData.isEmpty()) candidates.append(v.romData);
                    }
                }
            } catch (...) {}

            // ── Byte-match with anchor-based offset search ─────────────
            //
            // The needle and a candidate often differ in size (a small
            // calibration partition vs a 5-8 MB bench-full dump, or the
            // reverse).  Treat the SMALLER buffer as the probe and find
            // where it best aligns inside the LARGER one, then report
            // match% over the smaller buffer (a containment-style score).
            //
            // Alignment offsets come from CONTENT ANCHORS — a handful of
            // non-padding 24-byte sequences from the probe, located in the
            // larger buffer via indexOf — plus offset 0 and a coarse 64 KB
            // grid as fallback.  Anchors catch odd shifts (80-byte headers,
            // 0x1000, atypical layouts) that a fixed-grid slide would miss.
            auto bestAlign = [](const QByteArray &big,
                                const QByteArray &small) -> std::pair<int, int> {
                const int bigN = big.size(), smallN = small.size();
                if (smallN == 0 || smallN > bigN) return {0, 0};
                const auto *pb = reinterpret_cast<const uint8_t *>(big.constData());
                const auto *ps = reinterpret_cast<const uint8_t *>(small.constData());

                QVector<int> offs;
                offs.append(0);
                constexpr int kAnchorLen = 24;
                if (smallN >= kAnchorLen) {
                    constexpr int kAnchors = 8;
                    for (int a = 0; a < kAnchors; ++a) {
                        const int sp = int(qint64(a) * (smallN - kAnchorLen)
                                           / qMax(1, kAnchors - 1));
                        bool uniform = true;     // skip padding anchors
                        for (int k = 1; k < kAnchorLen; ++k)
                            if (ps[sp + k] != ps[sp]) { uniform = false; break; }
                        if (uniform) continue;
                        const QByteArray anchor =
                            QByteArray::fromRawData(small.constData() + sp, kAnchorLen);
                        int idx = big.indexOf(anchor), hits = 0;
                        while (idx >= 0 && hits < 4) {
                            const int off = idx - sp;
                            if (off >= 0 && off + smallN <= bigN) offs.append(off);
                            idx = big.indexOf(anchor, idx + 1);
                            ++hits;
                        }
                    }
                }
                for (int off = 0; off + smallN <= bigN; off += 64 * 1024)
                    offs.append(off);
                std::sort(offs.begin(), offs.end());
                offs.erase(std::unique(offs.begin(), offs.end()), offs.end());

                // Rank offsets by a sampled score, then full-score the top few.
                QVector<QPair<int, int>> sampled;   // (sampledMatch, off)
                for (int off : offs) {
                    int m = 0;
                    for (int i = 0; i < smallN; i += 64)
                        if (pb[off + i] == ps[i]) ++m;
                    sampled.append({m, off});
                }
                std::sort(sampled.begin(), sampled.end(),
                          [](const QPair<int,int> &a, const QPair<int,int> &b) {
                              return a.first > b.first;
                          });
                int bestM = 0;
                const int K = qMin(4, sampled.size());
                for (int t = 0; t < K; ++t) {
                    const int off = sampled[t].second;
                    int m = 0;
                    for (int i = 0; i < smallN; ++i)
                        if (pb[off + i] == ps[i]) ++m;
                    if (m > bestM) bestM = m;
                }
                return {bestM, smallN};
            };

            int bestMatch = 0, bestN = 0;
            double bestFrac = -1.0;
            for (const QByteArray &cand : candidates) {
                if (cand.isEmpty()) continue;
                const bool candBigger = cand.size() >= needle.size();
                const QByteArray &big   = candBigger ? cand : needle;
                const QByteArray &small = candBigger ? needle : cand;
                auto [m, tot] = bestAlign(big, small);
                if (tot <= 0) continue;
                const double frac = double(m) / double(tot);
                if (frac > bestFrac) { bestFrac = frac; bestMatch = m; bestN = tot; }
            }
            if (bestN <= 0) return;
            const double pct = double(bestMatch) * 100.0 / double(bestN);
            QMetaObject::invokeMethod(self.data(), [self, path, pct]() {
                if (self) self->onByteMatchResult(path, pct);
            }, Qt::QueuedConnection);
        });
    }
}

void SimilarFilesDlg::onByteMatchResult(const QString &path, double pct)
{
    m_byteMatchCache.insert(path, pct);
    if (!m_tree) return;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *it = m_tree->topLevelItem(i);
        if (it->data(0, Qt::UserRole).toString() != path) continue;
        it->setText(1, QStringLiteral("%1%").arg(pct, 0, 'f', 1));
        QColor c;
        if      (pct >= 95.0) c = QColor("#3fb950");  // green
        else if (pct >= 70.0) c = QColor("#d29922");  // amber
        else                  c = QColor("#f85149");  // red
        it->setForeground(1, c);
        break;
    }
}

}  // namespace winols
