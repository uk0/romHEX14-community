#pragma once
#include <QString>

namespace datalog {

enum class CanonicalUnit : int {
    Unknown,
    Dimensionless,    // lambda, fr, etc.
    Percent,
    AngleCrankDeg,    // ° / °CRK / Grad KW
    TempC,
    PressureBar,      // canonical: 1 bar
    MassFlowKgH,      // kg/h
    MassPerCycleMg,   // mg/Hub / mg/stroke
    Rpm,
    Milliseconds,
    Microseconds,
    Voltage,
    KilometersPerHour,
    Newton,           // Nm
    Gram,
    Binary            // 0/1
};

struct UnitInfo {
    CanonicalUnit canonical;
    double        scale;   // multiply raw value by this to obtain canonical-unit value
    QString       displayUnit; // pretty UI string e.g. "bar", "°C"
};

UnitInfo normalizeUnit(const QString &rawUnit);

} // namespace datalog
