#pragma once
#include "LogTable.h"
#include <QString>

namespace datalog {

enum class EcuFamily : int {
    Unknown,
    BoschMercME,        // "nmot_w" family — Mercedes Bosch ME (V12 / AMG / V8 / EDC)
    BmwMedc17,          // "Epm_nEng" — BMW B58/N20/B47/N47, Toyota Supra B58
    PorscheSdi21,       // "EnginRPM" CamelCase truncated — Cayenne 971, 991/992
    PorscheOldSdi,      // "gear" + "n" — older Porsche, [N] indices
    MercedesMeBlock,    // "Block031[…]" or "Motordrehzahl" — older MB ME9.7/M271/V12
    BmwMssS55,          // "nmot" without "_w" — M3 F80 / M4
    Autotuner           // Autotuner cloud logger — "timestamp" + EN names with "(unit)"
};

QString familyName(EcuFamily f);
EcuFamily detectFamily(const LogTable &t);

} // namespace datalog
