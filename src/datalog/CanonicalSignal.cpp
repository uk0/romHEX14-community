#include "CanonicalSignal.h"
#include <QCoreApplication>

namespace datalog {

// Use QCoreApplication::translate so lupdate can extract these strings.
#define TR(ctx, str) QCoreApplication::translate(ctx, str)

QString signalName(Signal s)
{
    switch (s) {
    case Signal::Time:                    return TR("Datalog", "Time");
    case Signal::EngineRpm:               return TR("Datalog", "Engine RPM");
    case Signal::EngineTorque:            return TR("Datalog", "Engine torque");
    case Signal::EngineTorqueSetpoint:    return TR("Datalog", "Engine torque setpoint");
    case Signal::IntakeManifoldPressure:  return TR("Datalog", "Boost (intake mfd)");
    case Signal::BoostSetpoint:           return TR("Datalog", "Boost setpoint");
    case Signal::AmbientPressure:         return TR("Datalog", "Ambient pressure");
    case Signal::LambdaActual:            return TR("Datalog", "Lambda actual");
    case Signal::LambdaTarget:            return TR("Datalog", "Lambda target");
    case Signal::LambdaController:        return TR("Datalog", "Lambda controller");
    case Signal::IntakeAirTemp:           return TR("Datalog", "Intake air temp");
    case Signal::CoolantTemp:             return TR("Datalog", "Coolant temp");
    case Signal::OilTemp:                 return TR("Datalog", "Oil temp");
    case Signal::OilPressure:             return TR("Datalog", "Oil pressure");
    case Signal::ThrottlePosition:        return TR("Datalog", "Throttle");
    case Signal::AcceleratorPedal:        return TR("Datalog", "Accelerator pedal");
    case Signal::RailPressureActual:      return TR("Datalog", "Rail pressure (actual)");
    case Signal::RailPressureSetpoint:    return TR("Datalog", "Rail pressure (setpoint)");
    case Signal::FuelLowPressureSetpoint: return TR("Datalog", "Fuel low pressure setpoint");
    case Signal::InjectionDuration:       return TR("Datalog", "Injection duration");
    case Signal::LongTermFuelTrim:        return TR("Datalog", "Long term fuel trim");
    case Signal::ShortTermFuelTrim:       return TR("Datalog", "Short term fuel trim");
    case Signal::IgnitionTimingOut:       return TR("Datalog", "Ignition timing");
    case Signal::KnockRetardCyl:          return TR("Datalog", "Knock retard");
    case Signal::KnockFlag:               return TR("Datalog", "Knock detected");
    case Signal::MisfireCounter:          return TR("Datalog", "Misfire counter");
    case Signal::WastegatePos:            return TR("Datalog", "Wastegate");
    case Signal::MassAirFlow:             return TR("Datalog", "MAF");
    case Signal::MassAirFlowSetpoint:     return TR("Datalog", "MAF setpoint");
    case Signal::RelativeLoad:            return TR("Datalog", "Relative load");
    case Signal::Gear:                    return TR("Datalog", "Gear");
    case Signal::ExhaustGasTemp:          return TR("Datalog", "Exhaust temp");
    case Signal::VehicleSpeed:            return TR("Datalog", "Speed");
    case Signal::Unknown:                 return TR("Datalog", "(unknown)");
    }
    return TR("Datalog", "(unknown)");
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
        return TR("Datalog", "Engine");
    case Signal::IntakeManifoldPressure:
    case Signal::BoostSetpoint:
    case Signal::AmbientPressure:
    case Signal::WastegatePos:
    case Signal::IntakeAirTemp:
    case Signal::MassAirFlow:
    case Signal::MassAirFlowSetpoint:
    case Signal::RelativeLoad:
        return TR("Datalog", "Boost & Air");
    case Signal::LambdaActual:
    case Signal::LambdaTarget:
    case Signal::LambdaController:
    case Signal::ExhaustGasTemp:
    case Signal::LongTermFuelTrim:
    case Signal::ShortTermFuelTrim:
        return TR("Datalog", "Lambda & Exhaust");
    case Signal::IgnitionTimingOut:
    case Signal::KnockRetardCyl:
    case Signal::KnockFlag:
    case Signal::MisfireCounter:
        return TR("Datalog", "Ignition");
    case Signal::RailPressureActual:
    case Signal::RailPressureSetpoint:
    case Signal::FuelLowPressureSetpoint:
    case Signal::InjectionDuration:
        return TR("Datalog", "Fuel");
    case Signal::ThrottlePosition:
    case Signal::AcceleratorPedal:
    case Signal::Gear:
    case Signal::VehicleSpeed:
    case Signal::Time:
    case Signal::Unknown:
        return TR("Datalog", "Aux");
    }
    return TR("Datalog", "Aux");
}

#undef TR

} // namespace datalog
