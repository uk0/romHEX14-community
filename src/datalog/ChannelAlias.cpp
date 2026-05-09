#include "ChannelAlias.h"

#include <QRegularExpression>

namespace datalog {

namespace {

// Strip cylinder index from vendor channel name. Returns base name and sets *cyl
// to 1..N when found. Recognizes: "name Array [N]", "name[N]", "nameZylN",
// "nameCylinN", "name Cylinder N" (case-insensitive). For Mercedes
// "Block019[label]" the bracket content is non-numeric — we leave the name as-is
// and *cyl = -1.
QString stripCylinderIndex(const QString &name, int *cyl)
{
    *cyl = -1;
    static const QRegularExpression rxArray(
        QStringLiteral("\\s*Array\\s*\\[(\\d+)\\]\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rxBracket(
        QStringLiteral("\\[(\\d+)\\]\\s*$"));
    static const QRegularExpression rxZyl(
        QStringLiteral("(Zyl|Cylin|Cyl)(\\d+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    // Autotuner: "Ignition withdrawal timing Cylinder 1"
    static const QRegularExpression rxCylinder(
        QStringLiteral("\\s+Cylinder\\s+(\\d+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch m;
    m = rxArray.match(name);
    if (m.hasMatch()) { *cyl = m.captured(1).toInt() + 1; // 0-based -> 1-based
        return name.left(m.capturedStart()).trimmed(); }

    m = rxBracket.match(name);
    if (m.hasMatch()) {
        QString inside = m.captured(1);
        bool ok = false;
        int v = inside.toInt(&ok);
        if (ok) {
            *cyl = v + 1;   // 0-based "[0]" -> cylinder 1
            return name.left(m.capturedStart()).trimmed();
        }
    }

    m = rxCylinder.match(name);
    if (m.hasMatch()) { *cyl = m.captured(1).toInt();
        return name.left(m.capturedStart()).trimmed(); }

    m = rxZyl.match(name);
    if (m.hasMatch()) { *cyl = m.captured(2).toInt();
        return name.left(m.capturedStart()).trimmed(); }

    return name;
}

// Build the alias table once, lazily.
struct AliasKey {
    EcuFamily family;
    QString   base;   // base name (no cyl index)
};
inline bool operator==(const AliasKey &a, const AliasKey &b) {
    return a.family == b.family && a.base == b.base;
}
inline size_t qHash(const AliasKey &k, size_t seed = 0) noexcept {
    return ::qHash(int(k.family), seed) ^ ::qHash(k.base, seed);
}

const QHash<AliasKey, Signal> &table()
{
    static const QHash<AliasKey, Signal> t = []{
        QHash<AliasKey, Signal> m;
        auto add = [&](EcuFamily f, std::initializer_list<const char *> names, Signal s){
            for (auto n : names) m.insert({f, QString::fromLatin1(n)}, s);
        };

        // ===== BMW MEDC17 =====
        add(EcuFamily::BmwMedc17, {"Epm_nEng"}, Signal::EngineRpm);
        add(EcuFamily::BmwMedc17, {"mkist_w_msg", "ActMod_trqClth", "ActMod_trqClthWoDstC", "ActMod_trq"}, Signal::EngineTorque);
        add(EcuFamily::BmwMedc17, {"ps_w_msg", "rl_msg"}, Signal::IntakeManifoldPressure);
        add(EcuFamily::BmwMedc17, {"psolldr_w_msg", "rlsol_w_msg"}, Signal::BoostSetpoint);
        add(EcuFamily::BmwMedc17, {"UEGO_rLamS1B1", "lamsoni_w_msg"}, Signal::LambdaActual);
        add(EcuFamily::BmwMedc17, {"lamsons_w_msg", "lamsbg_w_msg"}, Signal::LambdaTarget);
        add(EcuFamily::BmwMedc17, {"fr_w_msg"}, Signal::LambdaController);
        add(EcuFamily::BmwMedc17, {"tans_msg", "tans"}, Signal::IntakeAirTemp);
        add(EcuFamily::BmwMedc17, {"tmot_msg", "tmot"}, Signal::CoolantTemp);
        add(EcuFamily::BmwMedc17, {"toel_msg"}, Signal::OilTemp);
        add(EcuFamily::BmwMedc17, {"poel_w_msg"}, Signal::OilPressure);
        add(EcuFamily::BmwMedc17, {"wdks_msg", "wdkba_msg", "tvldste_w_msg"}, Signal::ThrottlePosition);
        add(EcuFamily::BmwMedc17, {"FuPHi_pRail", "prist_w_msg", "RailP_pCurr"}, Signal::RailPressureActual);
        add(EcuFamily::BmwMedc17, {"FuPHi_pRailSp", "prsoll_w_msg", "Rail_pSetPoint", "PCR_pDesVal"}, Signal::RailPressureSetpoint);
        add(EcuFamily::BmwMedc17, {"zwsol_msg", "zwoutakt_msg"}, Signal::IgnitionTimingOut);
        add(EcuFamily::BmwMedc17, {"BasSvrAppl_AusZun_msg", "Zuendwinkelspaetverstellung_HWZyl"}, Signal::KnockRetardCyl);
        add(EcuFamily::BmwMedc17, {"SwSABMW_ratPosWgeTst"}, Signal::WastegatePos);
        add(EcuFamily::BmwMedc17, {"ml_msg"}, Signal::MassAirFlow);
        add(EcuFamily::BmwMedc17, {"rl_msg"}, Signal::RelativeLoad);
        add(EcuFamily::BmwMedc17, {"gangi_msg"}, Signal::Gear);
        add(EcuFamily::BmwMedc17, {"ExhMod_tExhS2B1", "Exh_tAdapTOxiCatUs"}, Signal::ExhaustGasTemp);

        // ===== Bosch MercME =====
        add(EcuFamily::BoschMercME, {"nmot_w"}, Signal::EngineRpm);
        add(EcuFamily::BoschMercME, {"Eng_Trq"}, Signal::EngineTorque);
        add(EcuFamily::BoschMercME, {"pvds_w"}, Signal::BoostSetpoint);
        add(EcuFamily::BoschMercME, {"pvd_w"}, Signal::IntakeManifoldPressure);
        add(EcuFamily::BoschMercME, {"lamsoni_w"}, Signal::LambdaActual);
        add(EcuFamily::BoschMercME, {"lamsons_w", "Lambda_Sollwert_Links"}, Signal::LambdaTarget);
        add(EcuFamily::BoschMercME, {"fr_w", "Lambdaregelwert_Links"}, Signal::LambdaController);
        add(EcuFamily::BoschMercME, {"tans"}, Signal::IntakeAirTemp);
        add(EcuFamily::BoschMercME, {"tmot"}, Signal::CoolantTemp);
        add(EcuFamily::BoschMercME, {"wdkba", "tvldste_w"}, Signal::ThrottlePosition);
        add(EcuFamily::BoschMercME, {"prist_w"}, Signal::RailPressureActual);
        add(EcuFamily::BoschMercME, {"prsoll_w"}, Signal::RailPressureSetpoint);
        add(EcuFamily::BoschMercME, {"zwout"}, Signal::IgnitionTimingOut);
        add(EcuFamily::BoschMercME, {"Zuendwinkelspaetverstellung_HWZyl", "wkrmv"}, Signal::KnockRetardCyl);
        add(EcuFamily::BoschMercME, {"rl_w"}, Signal::RelativeLoad);
        add(EcuFamily::BoschMercME, {"gentrq_w"}, Signal::EngineTorque); // generator torque is supplemental but useful

        // ===== Porsche SDI21 (CamelCase truncated) =====
        add(EcuFamily::PorscheSdi21, {"EnginRPM"}, Signal::EngineRpm);
        add(EcuFamily::PorscheSdi21, {"EnginTorqu"}, Signal::EngineTorque);
        add(EcuFamily::PorscheSdi21, {"IntakManifAirPressCommaValue", "IntakManifAirPressCorreValue"}, Signal::IntakeManifoldPressure);
        add(EcuFamily::PorscheSdi21, {"BoostPressActuaCommaValue", "BoostPressCommaValue"}, Signal::BoostSetpoint);
        add(EcuFamily::PorscheSdi21, {"BoostPress"}, Signal::IntakeManifoldPressure);
        add(EcuFamily::PorscheSdi21, {"UEGORLamS1B1"}, Signal::LambdaActual);
        add(EcuFamily::PorscheSdi21, {"FuelAirCommaEquivRatio"}, Signal::LambdaTarget);
        add(EcuFamily::PorscheSdi21, {"IntakManifAirTempeRawValue", "IntakManifAirTempeMeasuRawValue"}, Signal::IntakeAirTemp);
        add(EcuFamily::PorscheSdi21, {"EnginCoolaTempe"}, Signal::CoolantTemp);
        add(EcuFamily::PorscheSdi21, {"RelatThrotPosit", "ThrotActuaActuaValuePerce"}, Signal::ThrottlePosition);
        add(EcuFamily::PorscheSdi21, {"FuelHighPressActuaValue"}, Signal::RailPressureActual);
        add(EcuFamily::PorscheSdi21, {"FuelHighPressCommaValue"}, Signal::RailPressureSetpoint);
        add(EcuFamily::PorscheSdi21, {"IgnitTiminAdvanFor1Cylin"}, Signal::IgnitionTimingOut);
        add(EcuFamily::PorscheSdi21, {"IgnitAdvanReducCylin"}, Signal::KnockRetardCyl);
        add(EcuFamily::PorscheSdi21, {"AirMassRatedValue", "IntakManifModelAirMassFlowFilteValue"}, Signal::MassAirFlow);
        add(EcuFamily::PorscheSdi21, {"LoadValueRelat", "LoadValueCorreCommaValue"}, Signal::RelativeLoad);

        // ===== Porsche OldSDI =====
        add(EcuFamily::PorscheOldSdi, {"n"}, Signal::EngineRpm);
        add(EcuFamily::PorscheOldSdi, {"tq_av", "tqi_av"}, Signal::EngineTorque);
        add(EcuFamily::PorscheOldSdi, {"prs_im_mes", "prs_im_mdl"}, Signal::IntakeManifoldPressure);
        add(EcuFamily::PorscheOldSdi, {"prs_im_sp"}, Signal::BoostSetpoint);
        add(EcuFamily::PorscheOldSdi, {"pwm_etc"}, Signal::ThrottlePosition);
        add(EcuFamily::PorscheOldSdi, {"PWM_TUR_ACR", "PWM_TUR_ACR_SP"}, Signal::WastegatePos);
        add(EcuFamily::PorscheOldSdi, {"tia"}, Signal::IntakeAirTemp);
        add(EcuFamily::PorscheOldSdi, {"toil_sens"}, Signal::OilTemp);
        add(EcuFamily::PorscheOldSdi, {"gear"}, Signal::Gear);

        // ===== Mercedes ME (block-based) =====
        add(EcuFamily::MercedesMeBlock, {"Block031[Motordrehzahl]", "Motordrehzahl"}, Signal::EngineRpm);
        add(EcuFamily::MercedesMeBlock, {"Block029[TQI_AV]"}, Signal::EngineTorque);
        add(EcuFamily::MercedesMeBlock, {"PUT_MES", "Ladedruck", "MAP_MES", "Block003[Saugrohrdruck]"}, Signal::IntakeManifoldPressure);
        add(EcuFamily::MercedesMeBlock, {"Ladedruck_Soll"}, Signal::BoostSetpoint);
        add(EcuFamily::MercedesMeBlock, {"Block019[MAF_KGH_MES]"}, Signal::MassAirFlow);
        add(EcuFamily::MercedesMeBlock, {"PWM_WG"}, Signal::WastegatePos);
        add(EcuFamily::MercedesMeBlock, {"TIA"}, Signal::IntakeAirTemp);
        add(EcuFamily::MercedesMeBlock, {"Zuendwinkel"}, Signal::IgnitionTimingOut);
        add(EcuFamily::MercedesMeBlock, {"Zuendwinkelspaetverstellung_HWZyl"}, Signal::KnockRetardCyl);

        // ===== Autotuner (cloud logger, EN names with units in parens) =====
        // Names below are MATCHED LOWERCASE — see resolve() below.
        add(EcuFamily::Autotuner, {"engine speed"}, Signal::EngineRpm);
        add(EcuFamily::Autotuner, {"engine torque"}, Signal::EngineTorque);
        add(EcuFamily::Autotuner, {"engine torque setpoint"}, Signal::EngineTorqueSetpoint);
        add(EcuFamily::Autotuner, {"boost pressure"}, Signal::IntakeManifoldPressure);
        add(EcuFamily::Autotuner, {"boost pressure setpoint"}, Signal::BoostSetpoint);
        add(EcuFamily::Autotuner, {"ambient pressure"}, Signal::AmbientPressure);
        add(EcuFamily::Autotuner, {"lambda (afr)", "lambda"}, Signal::LambdaActual);
        add(EcuFamily::Autotuner, {"lambda (afr) setpoint",
                                    "lambda (afr) bank 1 setpoint",
                                    "lambda setpoint",
                                    "lambda target"}, Signal::LambdaTarget);
        add(EcuFamily::Autotuner, {"intake air temperature"}, Signal::IntakeAirTemp);
        add(EcuFamily::Autotuner, {"engine temperature", "coolant temperature"}, Signal::CoolantTemp);
        add(EcuFamily::Autotuner, {"engine oil temperature", "oil temperature"}, Signal::OilTemp);
        add(EcuFamily::Autotuner, {"oil pressure"}, Signal::OilPressure);
        add(EcuFamily::Autotuner, {"throttle valve position"}, Signal::ThrottlePosition);
        add(EcuFamily::Autotuner, {"gaspedal position", "accelerator pedal position"}, Signal::AcceleratorPedal);
        add(EcuFamily::Autotuner, {"fuel high pressure"}, Signal::RailPressureActual);
        add(EcuFamily::Autotuner, {"fuel high pressure setpoint"}, Signal::RailPressureSetpoint);
        add(EcuFamily::Autotuner, {"fuel low pressure setpoint"}, Signal::FuelLowPressureSetpoint);
        add(EcuFamily::Autotuner, {"injection duration"}, Signal::InjectionDuration);
        add(EcuFamily::Autotuner, {"long term fuel trim"}, Signal::LongTermFuelTrim);
        add(EcuFamily::Autotuner, {"short term fuel trim"}, Signal::ShortTermFuelTrim);
        add(EcuFamily::Autotuner, {"ignition advance"}, Signal::IgnitionTimingOut);
        // Per-cylinder knock retard — resolve() strips "Cylinder N" suffix first.
        add(EcuFamily::Autotuner, {"ignition withdrawal timing"}, Signal::KnockRetardCyl);
        add(EcuFamily::Autotuner, {"knock detected"}, Signal::KnockFlag);
        add(EcuFamily::Autotuner, {"misfire counter"}, Signal::MisfireCounter);
        add(EcuFamily::Autotuner, {"mass air flow"}, Signal::MassAirFlow);
        add(EcuFamily::Autotuner, {"mass air flow setpoint"}, Signal::MassAirFlowSetpoint);
        add(EcuFamily::Autotuner, {"load actual", "relative load"}, Signal::RelativeLoad);
        add(EcuFamily::Autotuner, {"exhaust gas temperature before catalyst",
                                    "exhaust gas temperature"}, Signal::ExhaustGasTemp);
        add(EcuFamily::Autotuner, {"vehicle speed"}, Signal::VehicleSpeed);
        add(EcuFamily::Autotuner, {"battery voltage"}, Signal::Unknown); // logged but not canonicalized
        add(EcuFamily::Autotuner, {"gear"}, Signal::Gear);
        add(EcuFamily::Autotuner, {"wastegate duty cycle", "wastegate position"}, Signal::WastegatePos);

        // ===== BMW MSS S55 =====
        add(EcuFamily::BmwMssS55, {"nmot"}, Signal::EngineRpm);
        add(EcuFamily::BmwMssS55, {"mkist_w"}, Signal::EngineTorque);
        add(EcuFamily::BmwMssS55, {"ps_w"}, Signal::IntakeManifoldPressure);
        add(EcuFamily::BmwMssS55, {"psolldr_w"}, Signal::BoostSetpoint);
        add(EcuFamily::BmwMssS55, {"lamsoni_w"}, Signal::LambdaActual);
        add(EcuFamily::BmwMssS55, {"lamsons_w"}, Signal::LambdaTarget);
        add(EcuFamily::BmwMssS55, {"fr_w"}, Signal::LambdaController);
        add(EcuFamily::BmwMssS55, {"tans"}, Signal::IntakeAirTemp);
        add(EcuFamily::BmwMssS55, {"tmot"}, Signal::CoolantTemp);
        add(EcuFamily::BmwMssS55, {"wdk1", "tvldste_w"}, Signal::ThrottlePosition);
        add(EcuFamily::BmwMssS55, {"prist_w"}, Signal::RailPressureActual);
        add(EcuFamily::BmwMssS55, {"prsoll_w"}, Signal::RailPressureSetpoint);
        add(EcuFamily::BmwMssS55, {"zwzyl1", "zwoutzyl_w"}, Signal::IgnitionTimingOut);
        add(EcuFamily::BmwMssS55, {"zwoutzyln_w"}, Signal::KnockRetardCyl);
        add(EcuFamily::BmwMssS55, {"ml"}, Signal::MassAirFlow);
        add(EcuFamily::BmwMssS55, {"rl"}, Signal::RelativeLoad);

        return m;
    }();
    return t;
}

} // namespace

AliasMatch ChannelAlias::resolve(const QString &vendorName, EcuFamily family)
{
    AliasMatch r{ Signal::Unknown, -1 };
    if (vendorName == QLatin1String("Time")) { r.signal = Signal::Time; return r; }

    int cyl = -1;
    QString base = stripCylinderIndex(vendorName, &cyl);
    const auto &t = table();

    // Autotuner uses long English phrases — match case-insensitively.
    QString lookup = (family == EcuFamily::Autotuner) ? base.toLower() : base;
    auto it = t.constFind({family, lookup});
    if (it != t.constEnd()) {
        r.signal = it.value();
        r.cylIndex = (r.signal == Signal::KnockRetardCyl) ? cyl : -1;
        return r;
    }
    // Unknown family fallback: try every family until we find one
    if (family == EcuFamily::Unknown) {
        for (auto fam : { EcuFamily::BmwMedc17, EcuFamily::BoschMercME,
                          EcuFamily::PorscheSdi21, EcuFamily::PorscheOldSdi,
                          EcuFamily::MercedesMeBlock, EcuFamily::BmwMssS55,
                          EcuFamily::Autotuner }) {
            QString k = (fam == EcuFamily::Autotuner) ? base.toLower() : base;
            it = t.constFind({fam, k});
            if (it != t.constEnd()) {
                r.signal = it.value();
                r.cylIndex = (r.signal == Signal::KnockRetardCyl) ? cyl : -1;
                return r;
            }
        }
    }
    return r;
}

QHash<int, AliasMatch> ChannelAlias::resolveAll(const LogTable &t, EcuFamily family)
{
    QHash<int, AliasMatch> out;
    out.reserve(t.columns.size());
    for (int i = 0; i < t.columns.size(); ++i)
        out.insert(i, resolve(t.columns[i].name, family));
    return out;
}

int ChannelAlias::findColumn(const LogTable &t, EcuFamily family, Signal s, int cylIndex)
{
    for (int i = 0; i < t.columns.size(); ++i) {
        AliasMatch m = resolve(t.columns[i].name, family);
        if (m.signal != s) continue;
        if (s == Signal::KnockRetardCyl && cylIndex >= 0 && m.cylIndex != cylIndex) continue;
        return i;
    }
    return -1;
}

} // namespace datalog
