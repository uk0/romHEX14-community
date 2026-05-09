#pragma once
#include <QDialog>

#include "LogTable.h"
#include "EcuFamily.h"
#include "PullDetector.h"

class QTableWidget;
class QPushButton;
class QLabel;

namespace datalog {

class TimeSeriesPlotWidget;

class CompareLogsDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompareLogsDialog(QWidget *parent = nullptr);

    bool loadPair(const QString &pathA, const QString &pathB, QString *err = nullptr);

private slots:
    void onPickA();
    void onPickB();

private:
    void buildUi();
    void recompute();
    int  bestPullIndex(const QVector<Pull> &pulls) const;

    QString m_pathA, m_pathB;
    LogTable m_a, m_b;
    EcuFamily m_familyA = EcuFamily::Unknown;
    EcuFamily m_familyB = EcuFamily::Unknown;
    QVector<Pull>      m_pullsA, m_pullsB;
    QVector<PullStats> m_statsA, m_statsB;

    QPushButton  *m_pickA = nullptr;
    QPushButton  *m_pickB = nullptr;
    QTableWidget *m_diffTable = nullptr;
    QLabel       *m_summaryLabel = nullptr;
};

} // namespace datalog
