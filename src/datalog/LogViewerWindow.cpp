#include "LogViewerWindow.h"

#include "LogReader.h"
#include "ChannelAlias.h"
#include "TimeSeriesPlotWidget.h"
#include "ChannelTreeWidget.h"
#include "CanonicalSignal.h"

#include <QDockWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QStatusBar>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

namespace datalog {

LogViewerWindow::LogViewerWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Datalog Viewer"));
    resize(1200, 800);
    buildUi();

    QSettings s;
    restoreGeometry(s.value(QStringLiteral("datalog/viewerGeometry")).toByteArray());
    restoreState(s.value(QStringLiteral("datalog/viewerState")).toByteArray());
}

void LogViewerWindow::buildUi()
{
    m_plot = new TimeSeriesPlotWidget(this);
    setCentralWidget(m_plot);
    connect(m_plot, &TimeSeriesPlotWidget::crosshairMoved,
            this, &LogViewerWindow::onCrosshairMoved);
    connect(m_plot, &TimeSeriesPlotWidget::crosshairValues,
            this, &LogViewerWindow::onCrosshairValues);

    // Channel tree (left)
    m_tree = new ChannelTreeWidget(this);
    auto *dockTree = new QDockWidget(tr("Channels"), this);
    dockTree->setObjectName(QStringLiteral("dock_channels"));
    dockTree->setWidget(m_tree);
    addDockWidget(Qt::LeftDockWidgetArea, dockTree);
    connect(m_tree, &ChannelTreeWidget::selectionChanged,
            this, &LogViewerWindow::onChannelSelectionChanged);

    // Pull list + stats (right)
    m_pullList = new QTableWidget(this);
    m_pullList->setColumnCount(5);
    m_pullList->setHorizontalHeaderLabels({tr("#"), tr("Time"), tr("RPM"), tr("Δrpm"), tr("Gear")});
    m_pullList->horizontalHeader()->setStretchLastSection(true);
    m_pullList->verticalHeader()->setVisible(false);
    m_pullList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pullList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pullList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_pullList, &QTableWidget::cellClicked,
            this, [this](int r, int){ onPullClicked(r); });

    m_pullStatsTable = new QTableWidget(this);
    m_pullStatsTable->setColumnCount(2);
    m_pullStatsTable->setHorizontalHeaderLabels({tr("Stat"), tr("Value")});
    m_pullStatsTable->horizontalHeader()->setStretchLastSection(true);
    m_pullStatsTable->verticalHeader()->setVisible(false);
    m_pullStatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *dockPulls = new QDockWidget(tr("Pulls"), this);
    dockPulls->setObjectName(QStringLiteral("dock_pulls"));
    dockPulls->setWidget(m_pullList);
    addDockWidget(Qt::RightDockWidgetArea, dockPulls);

    auto *dockStats = new QDockWidget(tr("Pull stats"), this);
    dockStats->setObjectName(QStringLiteral("dock_pullstats"));
    dockStats->setWidget(m_pullStatsTable);
    addDockWidget(Qt::RightDockWidgetArea, dockStats);

    // Sidecars (bottom)
    m_dtcTable = new QTableWidget(this);
    m_dtcTable->setColumnCount(4);
    m_dtcTable->setHorizontalHeaderLabels({tr("Code"), tr("OBD"), tr("Description"), tr("Status")});
    m_dtcTable->horizontalHeader()->setStretchLastSection(true);
    m_dtcTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_idTable = new QTableWidget(this);
    m_idTable->setColumnCount(2);
    m_idTable->setHorizontalHeaderLabels({tr("Key"), tr("Value")});
    m_idTable->horizontalHeader()->setStretchLastSection(true);
    m_idTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *dockDtc = new QDockWidget(tr("DTCs"), this);
    dockDtc->setObjectName(QStringLiteral("dock_dtc"));
    dockDtc->setWidget(m_dtcTable);
    addDockWidget(Qt::BottomDockWidgetArea, dockDtc);

    auto *dockId = new QDockWidget(tr("ECU ID"), this);
    dockId->setObjectName(QStringLiteral("dock_ecuid"));
    dockId->setWidget(m_idTable);
    addDockWidget(Qt::BottomDockWidgetArea, dockId);

    // Toolbar
    auto *tb = addToolBar(tr("View"));
    tb->setObjectName(QStringLiteral("tb_view"));
    auto *resetAct = tb->addAction(tr("Reset zoom"));
    connect(resetAct, &QAction::triggered, this, &LogViewerWindow::onResetView);

    // Status bar
    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);
}

bool LogViewerWindow::openFile(const QString &path, QString *err)
{
    QString readErr;
    m_table = LogReader::read(path, &readErr);
    if (m_table.isEmpty()) {
        if (err) *err = readErr;
        return false;
    }

    m_family = detectFamily(m_table);
    m_pulls = PullDetector::detect(m_table, m_family);
    m_pullStats = PullDetector::statsForAll(m_pulls, m_table, m_family);
    m_dtc = SidecarLoader::readDtc(path);
    m_id  = SidecarLoader::readId(path);

    m_tree->setTable(&m_table, m_family);
    m_plot->setTable(&m_table);

    QFileInfo fi(path);
    setWindowTitle(tr("Datalog Viewer — %1  [%2]").arg(fi.fileName(), familyName(m_family)));

    populatePullList();
    populateSidecars();
    preselectDefaultChannels();

    statusBar()->showMessage(
        tr("%1 rows · %2 channels · %3 pulls · family: %4")
            .arg(m_table.rowCount()).arg(m_table.colCount() - 1)
            .arg(m_pulls.size()).arg(familyName(m_family)),
        4000);
    return true;
}

void LogViewerWindow::preselectDefaultChannels()
{
    // Pre-check RPM + Boost + Lambda actual + Throttle if present.
    QVector<int> wanted;
    for (auto sig : { Signal::EngineRpm, Signal::IntakeManifoldPressure,
                      Signal::LambdaActual, Signal::ThrottlePosition,
                      Signal::IgnitionTimingOut }) {
        int c = ChannelAlias::findColumn(m_table, m_family, sig);
        if (c > 0) wanted.push_back(c);
    }
    // Fallback: if cluster unknown — just RPM-like first column
    if (wanted.isEmpty() && m_table.colCount() > 1) wanted.push_back(1);

    for (int top = 0; top < m_tree->topLevelItemCount(); ++top) {
        auto *grp = m_tree->topLevelItem(top);
        for (int j = 0; j < grp->childCount(); ++j) {
            auto *ch = grp->child(j);
            int colIdx = ch->data(0, Qt::UserRole).toInt();
            if (wanted.contains(colIdx))
                ch->setCheckState(0, Qt::Checked);
        }
    }
}

void LogViewerWindow::populatePullList()
{
    m_pullList->setRowCount(m_pulls.size());
    for (int i = 0; i < m_pulls.size(); ++i) {
        const Pull &p = m_pulls[i];
        m_pullList->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        m_pullList->setItem(i, 1, new QTableWidgetItem(
            QStringLiteral("%1–%2 s").arg(p.timeStartMs / 1000.0, 0, 'f', 1)
                                     .arg(p.timeEndMs   / 1000.0, 0, 'f', 1)));
        m_pullList->setItem(i, 2, new QTableWidgetItem(
            QStringLiteral("%1→%2").arg(int(p.rpmStart)).arg(int(p.rpmPeak))));
        m_pullList->setItem(i, 3, new QTableWidgetItem(QString::number(int(p.rpmPeak - p.rpmStart))));
        m_pullList->setItem(i, 4, new QTableWidgetItem(
            (p.gearGuess > 0) ? QString::number(p.gearGuess) : QStringLiteral("?")));
    }
    m_pullList->resizeColumnsToContents();
    m_pullStatsTable->setRowCount(0);
}

void LogViewerWindow::populateSidecars()
{
    m_dtcTable->setRowCount(m_dtc.size());
    for (int i = 0; i < m_dtc.size(); ++i) {
        const DtcEntry &d = m_dtc[i];
        m_dtcTable->setItem(i, 0, new QTableWidgetItem(d.numericCode));
        m_dtcTable->setItem(i, 1, new QTableWidgetItem(d.obdCode));
        m_dtcTable->setItem(i, 2, new QTableWidgetItem(d.description));
        m_dtcTable->setItem(i, 3, new QTableWidgetItem(
            (d.statusFlags.isEmpty() ? d.statusBits : d.statusFlags.join(QStringLiteral("; ")))));
    }
    m_dtcTable->resizeColumnsToContents();

    int rows = m_id.dids.size() + (m_id.vin.isEmpty() ? 0 : 1);
    m_idTable->setRowCount(rows);
    int r = 0;
    if (!m_id.vin.isEmpty()) {
        m_idTable->setItem(r, 0, new QTableWidgetItem(QStringLiteral("VIN")));
        m_idTable->setItem(r, 1, new QTableWidgetItem(m_id.vin));
        ++r;
    }
    for (auto it = m_id.dids.constBegin(); it != m_id.dids.constEnd(); ++it, ++r) {
        m_idTable->setItem(r, 0, new QTableWidgetItem(it.key()));
        m_idTable->setItem(r, 1, new QTableWidgetItem(it.value()));
    }
    m_idTable->resizeColumnsToContents();
}

void LogViewerWindow::onChannelSelectionChanged(const QVector<int> &cols)
{
    m_plot->setVisibleColumns(cols);
}

void LogViewerWindow::onPullClicked(int rowInList)
{
    if (rowInList < 0 || rowInList >= m_pulls.size()) return;
    const Pull &p = m_pulls[rowInList];
    // Add 1s padding on each side
    double pad = 1000.0;
    m_plot->setXRangeMs(std::max(0.0, p.timeStartMs - pad), p.timeEndMs + pad);

    const PullStats &s = m_pullStats[rowInList];
    m_pullStatsTable->setRowCount(0);
    auto add = [&](const QString &k, const QString &v){
        int r = m_pullStatsTable->rowCount();
        m_pullStatsTable->insertRow(r);
        m_pullStatsTable->setItem(r, 0, new QTableWidgetItem(k));
        m_pullStatsTable->setItem(r, 1, new QTableWidgetItem(v));
    };
    add(tr("Pull duration"),  QStringLiteral("%1 s").arg((p.timeEndMs - p.timeStartMs) / 1000.0, 0, 'f', 2));
    add(tr("RPM range"),      QStringLiteral("%1 → %2").arg(int(p.rpmStart)).arg(int(p.rpmPeak)));
    add(tr("Peak boost"),     QStringLiteral("%1 bar").arg(s.peakBoostBar, 0, 'f', 2));
    add(tr("RPM @ peak boost"), QString::number(int(s.rpmAtPeakBoost)));
    add(tr("Peak torque"),    QStringLiteral("%1 Nm").arg(s.peakTorqueNm, 0, 'f', 1));
    add(tr("Peak IAT"),       QStringLiteral("%1 °C").arg(s.peakIntakeAirTempC, 0, 'f', 1));
    add(tr("AFR @ peak boost"), QString::number(s.afrAtPeakBoost, 'f', 2));
    add(tr("Knock retard ∑"), QStringLiteral("%1 °·s").arg(s.knockRetardSumDeg, 0, 'f', 2));
    add(tr("Time @ WOT"),     QStringLiteral("%1 s").arg(s.timeAtFullThrottleMs / 1000.0, 0, 'f', 1));
    if (p.gearGuess > 0) add(tr("Gear"), QString::number(p.gearGuess));
    m_pullStatsTable->resizeColumnsToContents();
}

void LogViewerWindow::onCrosshairMoved(double timeMs)
{
    Q_UNUSED(timeMs); // values are shown by onCrosshairValues
}

void LogViewerWindow::onCrosshairValues(double timeMs, const QVector<QPair<int,double>> &values)
{
    QStringList parts;
    parts.append(QStringLiteral("t = %1 s").arg(timeMs / 1000.0, 0, 'f', 2));
    for (const auto &v : values) {
        if (v.first < 0 || v.first >= m_table.colCount()) continue;
        const LogColumn &c = m_table.columns[v.first];
        QString name = c.name;
        if (name.length() > 12) name = name.left(10) + QStringLiteral("..");
        double abs = std::abs(v.second);
        int prec = (abs >= 100) ? 1 : (abs >= 1) ? 2 : 3;
        parts.append(QStringLiteral("%1: %2 %3")
                     .arg(name)
                     .arg(v.second, 0, 'f', prec)
                     .arg(c.unitRaw));
    }
    m_statusLabel->setText(parts.join(QStringLiteral("   ")));
}

void LogViewerWindow::onResetView()
{
    m_plot->resetView();
}

} // namespace datalog
