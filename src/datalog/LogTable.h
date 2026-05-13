#pragma once
#include <QString>
#include <QVector>

namespace datalog {

struct LogColumn {
    QString name;        // raw vendor name e.g. "nmot_w" / "Epm_nEng" / "EnginRPM"
    QString description; // human-readable EN/DE
    QString unitRaw;     // "° KW" / "Grad C" / "MPa" / "" / ...
    int     index = -1;  // column index in the row vector
};

class LogTable {
public:
    QString sourcePath;
    QChar   delimiter;          // ',' (most files) or '\t' (3 of 122)
    QString encoding;           // "utf-8-sig" or "utf-8" or "cp1252"
    QVector<LogColumn> columns; // columns[0] is always Time
    QVector<double>    timeMs;  // length = numRows; mirrors data[0]
    QVector<QVector<double>> data; // [colIdx][rowIdx]

    int rowCount() const { return timeMs.size(); }
    int colCount() const { return columns.size(); }
    bool isEmpty() const { return columns.isEmpty() || timeMs.isEmpty(); }

    int  findColumn(const QString &name) const; // -1 if not found
    const QVector<double> &series(int colIdx) const { return data[colIdx]; }
};

} // namespace datalog
