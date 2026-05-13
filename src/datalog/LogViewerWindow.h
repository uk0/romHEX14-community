#pragma once
#include <QMainWindow>

#include "LogTable.h"
#include "EcuFamily.h"
#include "PullDetector.h"
#include "SidecarLoader.h"

class QTableWidget;
class QStatusBar;
class QLabel;

namespace datalog {

class TimeSeriesPlotWidget;
class ChannelTreeWidget;

class LogViewerWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LogViewerWindow(QWidget *parent = nullptr);

    bool openFile(const QString &path, QString *err = nullptr);
    QString sourcePath() const { return m_table.sourcePath; }

private slots:
    void onChannelSelectionChanged(const QVector<int> &cols);
    void onPullClicked(int rowInList);
    void onCrosshairMoved(double timeMs);
    void onResetView();

private:
    void buildUi();
    void populatePullList();
    void populateSidecars();
    void preselectDefaultChannels();

    LogTable                 m_table;
    EcuFamily                m_family = EcuFamily::Unknown;
    QVector<Pull>            m_pulls;
    QVector<PullStats>       m_pullStats;
    QVector<DtcEntry>        m_dtc;
    EcuId                    m_id;

    TimeSeriesPlotWidget    *m_plot = nullptr;
    ChannelTreeWidget       *m_tree = nullptr;
    QTableWidget            *m_pullList = nullptr;
    QTableWidget            *m_pullStatsTable = nullptr;
    QTableWidget            *m_dtcTable = nullptr;
    QTableWidget            *m_idTable = nullptr;
    QLabel                  *m_statusLabel = nullptr;
};

} // namespace datalog
