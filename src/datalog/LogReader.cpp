#include "LogReader.h"

#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QStringList>
#include <QRegularExpression>

namespace datalog {

namespace {

QString detectEncoding(const QByteArray &raw)
{
    if (raw.startsWith("\xEF\xBB\xBF")) return QStringLiteral("utf-8-sig");
    auto dec = QStringDecoder(QStringConverter::Utf8, QStringDecoder::Flag::Stateless);
    QString s = dec.decode(raw);
    if (dec.hasError()) return QStringLiteral("cp1252");
    return QStringLiteral("utf-8");
}

QChar detectDelimiter(const QString &firstLine)
{
    int nComma = firstLine.count(QLatin1Char(','));
    int nTab   = firstLine.count(QLatin1Char('\t'));
    int nSemi  = firstLine.count(QLatin1Char(';'));
    if (nTab > nComma && nTab > nSemi)  return QLatin1Char('\t');
    if (nSemi > nComma)                  return QLatin1Char(';');
    return QLatin1Char(',');
}

// RFC-4180 lite: handle quoted fields with comma inside descriptions.
QStringList splitRow(const QString &line, QChar delim)
{
    QStringList out;
    QString cur;
    cur.reserve(line.size());
    bool inQuote = false;
    for (int i = 0; i < line.size(); ++i) {
        QChar c = line[i];
        if (inQuote) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line[i+1] == QLatin1Char('"')) {
                    cur.append(QLatin1Char('"')); ++i;
                } else {
                    inQuote = false;
                }
            } else {
                cur.append(c);
            }
        } else {
            if (c == QLatin1Char('"')) {
                inQuote = true;
            } else if (c == delim) {
                out.push_back(cur);
                cur.clear();
            } else {
                cur.append(c);
            }
        }
    }
    out.push_back(cur);
    return out;
}

QString decodeText(const QByteArray &raw, const QString &encoding)
{
    if (encoding == QLatin1String("cp1252")) return QString::fromLatin1(raw);
    QByteArray body = raw;
    if (body.startsWith("\xEF\xBB\xBF")) body.remove(0, 3);
    auto dec = QStringDecoder(QStringConverter::Utf8);
    return dec.decode(body);
}

enum class SourceFormat { Vehical, Autotuner };

// Sniff whether the CSV is Autotuner (single header, "timestamp" first col,
// units embedded as "(unit)" at end of name) or Vehical (3-row header with
// separate description and unit rows).
SourceFormat sniff(const QStringList &headerLine0, const QStringList &maybeRow1)
{
    if (headerLine0.isEmpty()) return SourceFormat::Vehical;
    QString first = headerLine0.first().trimmed();
    // Strip surrounding quotes if any
    if (first.startsWith(QLatin1Char('"')) && first.endsWith(QLatin1Char('"')))
        first = first.mid(1, first.size() - 2);
    if (first.compare(QStringLiteral("timestamp"), Qt::CaseInsensitive) == 0)
        return SourceFormat::Autotuner;

    // If columns embed "(unit)" pattern broadly, also call it Autotuner.
    int withUnits = 0;
    static const QRegularExpression rxUnit(QStringLiteral("\\([^()]*\\)\\s*$"));
    for (const QString &c : headerLine0) {
        if (rxUnit.match(c).hasMatch()) ++withUnits;
    }
    if (headerLine0.size() >= 3 && withUnits * 2 >= headerLine0.size())
        return SourceFormat::Autotuner;

    // Heuristic: if row[1] contains mostly numeric tokens, this is single-header
    // (no description row); fall back to Autotuner-style parser.
    if (!maybeRow1.isEmpty()) {
        int numeric = 0;
        for (const QString &v : maybeRow1) {
            bool ok = false;
            v.toDouble(&ok);
            if (ok) ++numeric;
        }
        if (numeric >= maybeRow1.size() - 1 && numeric > 0)
            return SourceFormat::Autotuner;
    }

    return SourceFormat::Vehical;
}

LogTable parseVehical(const QStringList &lines, QChar delim, const QString &enc,
                     const QString &path, QString *err)
{
    LogTable t;
    t.sourcePath = path;
    t.encoding   = enc;
    t.delimiter  = delim;

    if (lines.size() < 4) {
        if (err) *err = QStringLiteral("file has fewer than 4 lines (need 3-row header + 1 data row)");
        return t;
    }

    QStringList names = splitRow(lines[0], delim);
    QStringList descs = splitRow(lines[1], delim);
    QStringList units = splitRow(lines[2], delim);

    int nCols = names.size();
    if (nCols < 2) {
        if (err) *err = QStringLiteral("header has %1 columns; need at least 2 (Time + 1 channel)").arg(nCols);
        return t;
    }
    if (names.first().trimmed().compare(QStringLiteral("Time"), Qt::CaseInsensitive) != 0) {
        if (err) *err = QStringLiteral("first column is '%1'; expected 'Time'").arg(names.first());
        return t;
    }

    t.columns.reserve(nCols);
    for (int i = 0; i < nCols; ++i) {
        LogColumn c;
        c.name        = names[i].trimmed();
        c.description = (i < descs.size()) ? descs[i].trimmed() : QString();
        c.unitRaw     = (i < units.size()) ? units[i].trimmed() : QString();
        c.index       = i;
        t.columns.push_back(c);
    }

    t.data.resize(nCols);
    int rows = lines.size() - 3;
    for (auto &col : t.data) col.reserve(rows);
    t.timeMs.reserve(rows);

    for (int r = 3; r < lines.size(); ++r) {
        const QString &line = lines[r];
        if (line.trimmed().isEmpty()) continue;
        QStringList vals = splitRow(line, delim);
        for (int c = 0; c < nCols; ++c) {
            double v = 0.0;
            if (c < vals.size()) {
                bool ok = false;
                v = vals[c].toDouble(&ok);
                if (!ok) v = 0.0;
            }
            t.data[c].push_back(v);
        }
        t.timeMs.push_back(t.data[0].back());
    }
    return t;
}

LogTable parseAutotuner(const QStringList &lines, QChar delim, const QString &enc,
                       const QString &path, QString *err)
{
    LogTable t;
    t.sourcePath = path;
    t.encoding   = enc;
    t.delimiter  = delim;

    if (lines.size() < 2) {
        if (err) *err = QStringLiteral("file has fewer than 2 lines (need header + 1 data row)");
        return t;
    }

    QStringList rawNames = splitRow(lines[0], delim);
    int nCols = rawNames.size();
    if (nCols < 2) {
        if (err) *err = QStringLiteral("header has %1 columns; need at least 2").arg(nCols);
        return t;
    }

    static const QRegularExpression rxUnit(QStringLiteral("^(.*?)\\s*\\(([^()]*)\\)\\s*$"));
    t.columns.reserve(nCols);
    for (int i = 0; i < nCols; ++i) {
        LogColumn c;
        c.index = i;
        QString raw = rawNames[i].trimmed();
        QRegularExpressionMatch m = rxUnit.match(raw);
        if (m.hasMatch()) {
            c.name    = m.captured(1).trimmed();
            c.unitRaw = m.captured(2).trimmed();
        } else {
            c.name    = raw;
            c.unitRaw = QString();
        }
        c.description = c.name;

        // Normalize the time column so the rest of the pipeline finds "Time".
        if (i == 0 && c.name.compare(QStringLiteral("timestamp"), Qt::CaseInsensitive) == 0) {
            c.name        = QStringLiteral("Time");
            c.description = QStringLiteral("Elapsed time");
            if (c.unitRaw.isEmpty()) c.unitRaw = QStringLiteral("ms");
        }
        t.columns.push_back(c);
    }

    t.data.resize(nCols);
    int rows = lines.size() - 1;
    for (auto &col : t.data) col.reserve(rows);
    t.timeMs.reserve(rows);

    for (int r = 1; r < lines.size(); ++r) {
        const QString &line = lines[r];
        if (line.trimmed().isEmpty()) continue;
        QStringList vals = splitRow(line, delim);
        for (int c = 0; c < nCols; ++c) {
            double v = 0.0;
            if (c < vals.size()) {
                bool ok = false;
                v = vals[c].toDouble(&ok);
                if (!ok) v = 0.0;
            }
            t.data[c].push_back(v);
        }
        t.timeMs.push_back(t.data[0].back());
    }
    return t;
}

} // namespace

LogTable LogReader::read(const QString &path, QString *err)
{
    LogTable t;
    t.sourcePath = path;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("cannot open: %1").arg(f.errorString());
        return t;
    }
    QByteArray raw = f.readAll();
    f.close();

    QString encoding = detectEncoding(raw.left(4096));
    QString text     = decodeText(raw, encoding);

    QStringList lines = text.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::KeepEmptyParts);
    while (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
    if (lines.size() < 2) {
        if (err) *err = QStringLiteral("file has fewer than 2 lines");
        return t;
    }

    QChar delim = detectDelimiter(lines[0]);
    QStringList line0 = splitRow(lines[0], delim);
    QStringList line1 = splitRow(lines[1], delim);
    SourceFormat fmt = sniff(line0, line1);

    if (fmt == SourceFormat::Autotuner)
        return parseAutotuner(lines, delim, encoding, path, err);
    return parseVehical(lines, delim, encoding, path, err);
}

} // namespace datalog
