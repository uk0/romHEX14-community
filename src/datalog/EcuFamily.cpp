#include "EcuFamily.h"

#include <QSet>

namespace datalog {

QString familyName(EcuFamily f)
{
    switch (f) {
    case EcuFamily::BoschMercME:      return QStringLiteral("Bosch Mercedes ME");
    case EcuFamily::BmwMedc17:        return QStringLiteral("BMW MEDC17");
    case EcuFamily::PorscheSdi21:     return QStringLiteral("Porsche Continental SDI21");
    case EcuFamily::PorscheOldSdi:    return QStringLiteral("Porsche older SDI / ME");
    case EcuFamily::MercedesMeBlock:  return QStringLiteral("Mercedes ME (block-based)");
    case EcuFamily::BmwMssS55:        return QStringLiteral("BMW MSS S55 (M3/M4 F8x)");
    case EcuFamily::Autotuner:        return QStringLiteral("Autotuner (cloud)");
    case EcuFamily::Unknown:          return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

EcuFamily detectFamily(const LogTable &t)
{
    QSet<QString> cols;
    for (const auto &c : t.columns) cols.insert(c.name);

    // Autotuner: long English column names (unit lives in unitRaw, separately).
    if (cols.contains(QStringLiteral("Engine speed")) ||
        cols.contains(QStringLiteral("Gaspedal position")))
        return EcuFamily::Autotuner;

    if (cols.contains(QStringLiteral("EnginRPM")))                  return EcuFamily::PorscheSdi21;
    if (cols.contains(QStringLiteral("Epm_nEng")))                  return EcuFamily::BmwMedc17;
    if (cols.contains(QStringLiteral("nmot_w")))                    return EcuFamily::BoschMercME;
    if (cols.contains(QStringLiteral("nmot")))                      return EcuFamily::BmwMssS55;

    if (cols.contains(QStringLiteral("gear")) && cols.contains(QStringLiteral("n")))
        return EcuFamily::PorscheOldSdi;

    for (const auto &c : t.columns) {
        if (c.name.startsWith(QStringLiteral("Block"))) return EcuFamily::MercedesMeBlock;
    }
    if (cols.contains(QStringLiteral("Motordrehzahl"))) return EcuFamily::MercedesMeBlock;

    return EcuFamily::Unknown;
}

} // namespace datalog
