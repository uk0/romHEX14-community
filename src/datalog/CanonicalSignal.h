#pragma once
#include <QString>

namespace datalog {

enum class Signal : int {
    Time,
    EngineRpm,
    EngineTorque,
    EngineTorqueSetpoint,
    IntakeManifoldPressure,   // boost (absolute)
    BoostSetpoint,
    AmbientPressure,
    LambdaActual,             // sensor 1 bank 1 by default
    LambdaTarget,
    LambdaController,
    IntakeAirTemp,
    CoolantTemp,
    OilTemp,
    OilPressure,
    ThrottlePosition,
    AcceleratorPedal,
    RailPressureActual,
    RailPressureSetpoint,
    FuelLowPressureSetpoint,
    InjectionDuration,
    LongTermFuelTrim,
    ShortTermFuelTrim,
    IgnitionTimingOut,
    KnockRetardCyl,           // per-cylinder; cylIndex carries the cylinder #
    KnockFlag,
    MisfireCounter,
    WastegatePos,
    MassAirFlow,
    MassAirFlowSetpoint,
    RelativeLoad,
    Gear,
    ExhaustGasTemp,
    VehicleSpeed,
    Unknown
};

QString signalName(Signal s);
QString signalCategory(Signal s);   // "Engine" / "Boost" / "Lambda" / "Ignition" / "Fuel" / "Aux"

} // namespace datalog
