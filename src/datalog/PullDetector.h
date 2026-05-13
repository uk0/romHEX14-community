#pragma once
#include "LogTable.h"
#include "EcuFamily.h"
#include "ChannelAlias.h"

#include <QVector>

namespace datalog {

struct Pull {
    int    rowStart   = 0;
    int    rowEnd     = 0;       // inclusive
    double timeStartMs = 0.0;
    double timeEndMs   = 0.0;
    double rpmStart    = 0.0;
    double rpmPeak     = 0.0;
    int    gearGuess   = -1;
};

struct PullStats {
    Pull   span;
    double peakBoostBar      = 0.0;   // canonical bar (whatever family — already converted)
    double peakTorqueNm      = 0.0;
    double peakIntakeAirTempC = 0.0;
    double afrAtPeakBoost    = 0.0;
    double knockRetardSumDeg = 0.0;
    double timeAtFullThrottleMs = 0.0;
    double rpmAtPeakBoost    = 0.0;
};

class PullDetector {
public:
    static QVector<Pull>     detect(const LogTable &t, EcuFamily family);
    static PullStats         statsFor(const Pull &p, const LogTable &t, EcuFamily family);
    static QVector<PullStats> statsForAll(const QVector<Pull> &pulls, const LogTable &t, EcuFamily family);
};

} // namespace datalog
