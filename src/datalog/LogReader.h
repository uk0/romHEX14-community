#pragma once
#include "LogTable.h"
#include <QString>

namespace datalog {

class LogReader {
public:
    // Reads a Vehical CSV/TSV log. On error returns an empty LogTable and
    // sets *err (when err != nullptr).
    //
    // Auto-detects:
    //   - encoding (utf-8 with BOM / utf-8 / cp1252 fallback)
    //   - delimiter (',' / '\t' / ';')
    //   - 3-row header: name / description / unit
    //
    // Numeric parsing tolerates empty cells (treated as 0.0). The first
    // column is required to be "Time" in milliseconds.
    static LogTable read(const QString &path, QString *err = nullptr);
};

} // namespace datalog
