#include "CanonicalSignal.h"

namespace datalog {

QString signalName(Signal s)
{
    switch (s) {
    case Signal::Time:                    return QStringLiteral("Time");
    case Signal::EngineRpm:               return QStringLiteral("Engine RPM");
    case Signal::EngineTorque:            return QStringLiteral("Engine torque");
    case Signal::EngineTorqueSetpoint:    return QStringLiteral("Engine torque setpoint");
    case Signal::IntakeManifoldPressure:  return QStringLiteral("Boost (intake mfd)");
    case Signal::BoostSetpoint:           return QStringLiteral("Boost setpoint");
    case Signal::AmbientPressure:         return QStringLiteral("Ambient pressure");
    case Signal::LambdaActual:            return QStringLiteral("Lambda actual");
    case Signal::LambdaTarget:            return QStringLiteral("Lambda target");
    case Signal::LambdaController:        return QStringLiteral("Lambda controller");
    case Signal::IntakeAirTemp:           return QStringLiteral("Intake air temp");
    case Signal::CoolantTemp:             return QStringLiteral("Coolant temp");
    case Signal::OilTemp:                 return QStringLiteral("Oil temp");
    case Signal::OilPressure:             return QStringLiteral("Oil pressure");
    case Signal::ThrottlePosition:        return QStringLiteral("Throttle");
    case Signal::AcceleratorPedal:        return QStringLiteral("Accelerator pedal");
    case Signal::RailPressureActual:      return QStringLiteral("Rail pressure (actual)");
    case Signal::RailPressureSetpoint:    return QStringLiteral("Rail pressure (setpoint)");
    case Signal::FuelLowPressureSetpoint: return QStringLiteral("Fuel low pressure setpoint");
    case Signal::InjectionDuration:       return QStringLiteral("Injection duration");
    case Signal::LongTermFuelTrim:        return QStringLiteral("Long term fuel trim");
    case Signal::ShortTermFuelTrim:       return QStringLiteral("Short term fuel trim");
    case Signal::IgnitionTimingOut:       return QStringLiteral("Ignition timing");
    case Signal::KnockRetardCyl:          return QStringLiteral("Knock retard");
    case Signal::KnockFlag:               return QStringLiteral("Knock detected");
    case Signal::MisfireCounter:          return QStringLiteral("Misfire counter");
    case Signal::WastegatePos:            return QStringLiteral("Wastegate");
    case Signal::MassAirFlow:             return QStringLiteral("MAF");
    case Signal::MassAirFlowSetpoint:     return QStringLiteral("MAF setpoint");
    case Signal::RelativeLoad:            return QStringLiteral("Relative load");
    case Signal::Gear:                    return QStringLiteral("Gear");
    case Signal::ExhaustGasTemp:          return QStringLiteral("Exhaust temp");
    case Signal::VehicleSpeed:            return QStringLiteral("Speed");
    case Signal::Unknown:                 return QStringLiteral("(unknown)");
    }
    return QStringLiteral("(unknown)");
}

QString signalCategory(Signal s)
{
    switch (s) {
    case Signal::EngineRpm:
    case Signal::EngineTorque:
    case Signal::EngineTorqueSetpoint:
    case Signal::CoolantTemp:
    case Signal::OilTemp:
    case Signal::OilPressure:
        return QStringLiteral("Engine");
    case Signal::IntakeManifoldPressure:
    case Signal::BoostSetpoint:
    case Signal::AmbientPressure:
    case Signal::WastegatePos:
    case Signal::IntakeAirTemp:
    case Signal::MassAirFlow:
    case Signal::MassAirFlowSetpoint:
    case Signal::RelativeLoad:
        return QStringLiteral("Boost & Air");
    case Signal::LambdaActual:
    case Signal::LambdaTarget:
    case Signal::LambdaController:
    case Signal::ExhaustGasTemp:
    case Signal::LongTermFuelTrim:
    case Signal::ShortTermFuelTrim:
        return QStringLiteral("Lambda & Exhaust");
    case Signal::IgnitionTimingOut:
    case Signal::KnockRetardCyl:
    case Signal::KnockFlag:
    case Signal::MisfireCounter:
        return QStringLiteral("Ignition");
    case Signal::RailPressureActual:
    case Signal::RailPressureSetpoint:
    case Signal::FuelLowPressureSetpoint:
    case Signal::InjectionDuration:
        return QStringLiteral("Fuel");
    case Signal::ThrottlePosition:
    case Signal::AcceleratorPedal:
    case Signal::Gear:
    case Signal::VehicleSpeed:
    case Signal::Time:
    case Signal::Unknown:
        return QStringLiteral("Aux");
    }
    return QStringLiteral("Aux");
}

} // namespace datalog
