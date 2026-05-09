#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>

namespace datalog {

struct DtcEntry {
    QString numericCode;     // "4523"
    QString obdCode;         // "P1946"
    QString label;           // "FanCtlr1EE"
    QString description;     // "Coolant fan control module 1 …"
    QString statusBits;      // "00100000"
    QStringList statusFlags; // ["not active", "active at least once since …", "MIL off"]
};

struct EcuId {
    QString vin;
    QMap<QString, QString> dids; // CVN, Diagnostic ID, Part Number, Bootloader, ASW, Calibration, Part Version
};

struct LogProfile {
    QStringList channels; // ordered, matches CSV columns 1..N (Time excluded)
};

class SidecarLoader {
public:
    // Resolves <basename>_DTC.txt next to the source CSV. Empty vector if not found.
    static QVector<DtcEntry> readDtc(const QString &csvPath, QString *err = nullptr);

    // Resolves any "ID*.txt" in the same directory. Empty EcuId if not found.
    static EcuId readId(const QString &csvPath, QString *err = nullptr);

    // Resolves <basename>.logprof or first *.logprof in the same directory.
    // Empty profile if not found.
    static LogProfile readProfile(const QString &csvPath, QString *err = nullptr);
};

} // namespace datalog
