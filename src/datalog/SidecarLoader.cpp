#include "SidecarLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QTextStream>

namespace datalog {

namespace {

QString readTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray raw = f.readAll();
    if (raw.startsWith("\xEF\xBB\xBF")) raw.remove(0, 3);
    return QString::fromUtf8(raw);
}

} // namespace

QVector<DtcEntry> SidecarLoader::readDtc(const QString &csvPath, QString *err)
{
    QFileInfo fi(csvPath);
    QString candidate = fi.dir().filePath(fi.completeBaseName() + QStringLiteral("_DTC.txt"));
    if (!QFileInfo(candidate).exists()) {
        if (err) *err = QStringLiteral("no _DTC.txt sidecar next to %1").arg(fi.fileName());
        return {};
    }
    QString text = readTextFile(candidate);
    if (text.isEmpty()) return {};

    // Split into sections separated by blank lines.
    QVector<DtcEntry> out;
    QStringList sections;
    {
        QStringList buf;
        for (const QString &line : text.split(QRegularExpression(QStringLiteral("\\r?\\n")))) {
            if (line.trimmed().isEmpty()) {
                if (!buf.isEmpty()) { sections.push_back(buf.join(QLatin1Char('\n'))); buf.clear(); }
            } else {
                buf.push_back(line);
            }
        }
        if (!buf.isEmpty()) sections.push_back(buf.join(QLatin1Char('\n')));
    }

    // First section is "N DTC entry/entries." — skip when matched.
    static const QRegularExpression rxHeader(QStringLiteral("^\\s*\\d+\\s+DTC\\s+entr"),
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rxBitmask(QStringLiteral("^[01]{8}$"));

    for (const QString &sec : sections) {
        QStringList lines = sec.split(QLatin1Char('\n'));
        if (!lines.isEmpty() && rxHeader.match(lines.first()).hasMatch()) continue;
        if (lines.isEmpty()) continue;

        DtcEntry e;
        // Line 1: "<numeric>    <description>"
        QString l1 = lines[0].trimmed();
        int sep = l1.indexOf(QRegularExpression(QStringLiteral("\\s{2,}")));
        if (sep > 0) {
            e.numericCode = l1.left(sep).trimmed();
            e.description = l1.mid(sep).trimmed();
        } else {
            e.numericCode = l1;
        }
        // Line 2: "P<hex>    <label>" or "P<hex>    not active"
        if (lines.size() > 1) {
            QString l2 = lines[1].trimmed();
            int s2 = l2.indexOf(QRegularExpression(QStringLiteral("\\s{2,}")));
            if (s2 > 0) {
                e.obdCode = l2.left(s2).trimmed();
                e.label   = l2.mid(s2).trimmed();
            } else {
                e.obdCode = l2;
            }
        }
        // Line 3: "<bitmask>    <status1>"  + remaining indented lines = additional flags
        for (int i = 2; i < lines.size(); ++i) {
            QString li = lines[i];
            QString trimmed = li.trimmed();
            if (trimmed.isEmpty()) continue;
            // Check for bitmask at start
            int s3 = trimmed.indexOf(QRegularExpression(QStringLiteral("\\s{2,}")));
            QString left = (s3 > 0) ? trimmed.left(s3).trimmed() : trimmed;
            if (rxBitmask.match(left).hasMatch() && e.statusBits.isEmpty()) {
                e.statusBits = left;
                if (s3 > 0) e.statusFlags.push_back(trimmed.mid(s3).trimmed());
            } else {
                e.statusFlags.push_back(trimmed);
            }
        }
        if (!e.numericCode.isEmpty() || !e.obdCode.isEmpty()) out.push_back(e);
    }
    return out;
}

EcuId SidecarLoader::readId(const QString &csvPath, QString *err)
{
    QFileInfo fi(csvPath);
    QDir dir = fi.dir();
    // Find first ID*.txt in the same directory.
    QStringList ids = dir.entryList({QStringLiteral("ID*.txt")}, QDir::Files | QDir::Readable);
    if (ids.isEmpty()) {
        if (err) *err = QStringLiteral("no ID*.txt next to %1").arg(fi.fileName());
        return {};
    }
    QString text = readTextFile(dir.filePath(ids.first()));
    if (text.isEmpty()) return {};

    EcuId out;
    static const QRegularExpression rxTimePrefix(QStringLiteral("^\\[\\d{2}:\\d{2}:\\d{2}\\]\\s*"));

    for (const QString &raw : text.split(QRegularExpression(QStringLiteral("\\r?\\n")))) {
        QString line = raw;
        line.remove(rxTimePrefix);
        line = line.trimmed();
        if (line.isEmpty()) continue;
        int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        QString key = line.left(colon).trimmed();
        QString val = line.mid(colon + 1).trimmed();
        if (key.isEmpty()) {
            // header-less first line — VIN
            if (out.vin.isEmpty()) out.vin = val;
        } else {
            out.dids.insert(key, val);
        }
    }
    return out;
}

LogProfile SidecarLoader::readProfile(const QString &csvPath, QString *err)
{
    QFileInfo fi(csvPath);
    QDir dir = fi.dir();
    QString candidate = dir.filePath(fi.completeBaseName() + QStringLiteral(".logprof"));
    if (!QFileInfo(candidate).exists()) {
        QStringList profs = dir.entryList({QStringLiteral("*.logprof")}, QDir::Files | QDir::Readable);
        if (profs.isEmpty()) {
            if (err) *err = QStringLiteral("no .logprof next to %1").arg(fi.fileName());
            return {};
        }
        candidate = dir.filePath(profs.first());
    }
    QString text = readTextFile(candidate);
    LogProfile out;
    for (const QString &line : text.split(QRegularExpression(QStringLiteral("\\r?\\n")))) {
        QString t = line.trimmed();
        if (t.isEmpty()) continue;
        int sep = t.indexOf(QLatin1Char(';'));
        QString name = (sep > 0) ? t.left(sep) : t;
        out.channels.push_back(name.trimmed());
    }
    return out;
}

} // namespace datalog
