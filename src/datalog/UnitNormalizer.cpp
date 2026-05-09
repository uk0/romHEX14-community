#include "UnitNormalizer.h"

#include <QHash>
#include <QStringList>

namespace datalog {

namespace {

// Lower-cases and collapses whitespace for fuzzy match against known synonyms.
QString canon(const QString &s)
{
    QString out;
    out.reserve(s.size());
    bool prevSpace = false;
    for (QChar c : s) {
        if (c.isSpace()) {
            if (!prevSpace && !out.isEmpty()) out.append(QLatin1Char(' '));
            prevSpace = true;
        } else {
            out.append(c.toLower());
            prevSpace = false;
        }
    }
    while (out.endsWith(QLatin1Char(' '))) out.chop(1);
    return out;
}

} // namespace

UnitInfo normalizeUnit(const QString &rawUnit)
{
    QString u = canon(rawUnit);
    // pre-normalize Unicode degree sign and micro
    u.replace(QStringLiteral("µ"), QStringLiteral("u")); // µ -> u

    auto eq = [&](std::initializer_list<const char *> opts) {
        for (auto o : opts) {
            if (u == QString::fromLatin1(o)) return true;
        }
        return false;
    };

    if (u.isEmpty() || u == QLatin1String("-") || u == QLatin1String("-0") || u == QLatin1String("lambda"))
        return { CanonicalUnit::Dimensionless, 1.0, QString() };

    // angle (crank)
    if (eq({"°", "°kw", "° kw", "°crk", "grad", "grad kw", "°ckr", "° crk"}))
        return { CanonicalUnit::AngleCrankDeg, 1.0, QStringLiteral("°") };

    // temperature
    if (eq({"°c", "° c", "degc", "deg c", "grad c"}))
        return { CanonicalUnit::TempC, 1.0, QStringLiteral("°C") };

    // pressure -> canonical bar
    if (eq({"bar"}))   return { CanonicalUnit::PressureBar, 1.0,    QStringLiteral("bar") };
    if (eq({"mpa"}))   return { CanonicalUnit::PressureBar, 10.0,   QStringLiteral("bar") };
    if (eq({"hpa", "mbar"})) return { CanonicalUnit::PressureBar, 0.001,  QStringLiteral("bar") };
    if (eq({"kpa"}))   return { CanonicalUnit::PressureBar, 0.01,   QStringLiteral("bar") };
    if (eq({"psi"}))   return { CanonicalUnit::PressureBar, 0.0689476, QStringLiteral("bar") };

    // mass flow
    if (eq({"kg/h"})) return { CanonicalUnit::MassFlowKgH, 1.0, QStringLiteral("kg/h") };

    // mass per cycle
    if (eq({"mg/hub", "mg/stroke"}))
        return { CanonicalUnit::MassPerCycleMg, 1.0, QStringLiteral("mg/cyc") };

    // throttle / load
    if (eq({"%", "% dk", "°dk"}))
        return { CanonicalUnit::Percent, 1.0, QStringLiteral("%") };

    // rpm
    if (eq({"1/min", "rpm"}))
        return { CanonicalUnit::Rpm, 1.0, QStringLiteral("rpm") };

    // time
    if (eq({"ms"})) return { CanonicalUnit::Milliseconds, 1.0, QStringLiteral("ms") };
    if (eq({"us"})) return { CanonicalUnit::Microseconds, 1.0, QStringLiteral("µs") };

    // voltage
    if (eq({"v"})) return { CanonicalUnit::Voltage, 1.0, QStringLiteral("V") };
    if (eq({"v*ms"})) return { CanonicalUnit::Voltage, 1.0, QStringLiteral("V*ms") };

    // speed
    if (eq({"km/h"})) return { CanonicalUnit::KilometersPerHour, 1.0, QStringLiteral("km/h") };

    // torque
    if (eq({"nm"})) return { CanonicalUnit::Newton, 1.0, QStringLiteral("Nm") };

    // misc
    if (eq({"g"})) return { CanonicalUnit::Gram, 1.0, QStringLiteral("g") };
    if (eq({"0/1"})) return { CanonicalUnit::Binary, 1.0, QString() };

    return { CanonicalUnit::Unknown, 1.0, rawUnit };
}

} // namespace datalog
