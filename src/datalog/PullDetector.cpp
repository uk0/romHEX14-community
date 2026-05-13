#include "PullDetector.h"
#include "UnitNormalizer.h"

#include <algorithm>
#include <limits>

namespace datalog {

namespace {

// Convert raw value at row r in column col to canonical-unit value (e.g. bar).
double valueCanonical(const LogTable &t, int col, int row)
{
    if (col < 0 || col >= t.colCount()) return 0.0;
    UnitInfo u = normalizeUnit(t.columns[col].unitRaw);
    return t.data[col][row] * u.scale;
}

double maxOf(const QVector<double> &v)
{
    double m = -std::numeric_limits<double>::infinity();
    for (double x : v) if (x > m) m = x;
    return m;
}

} // namespace

QVector<Pull> PullDetector::detect(const LogTable &t, EcuFamily family)
{
    QVector<Pull> out;
    if (t.rowCount() < 5) return out;

    int colTime = 0; // Time always col 0
    int colRpm = ChannelAlias::findColumn(t, family, Signal::EngineRpm);
    if (colRpm < 0) return out;

    int colLoad     = ChannelAlias::findColumn(t, family, Signal::RelativeLoad);
    int colThrottle = ChannelAlias::findColumn(t, family, Signal::ThrottlePosition);
    int colGear     = ChannelAlias::findColumn(t, family, Signal::Gear);

    const QVector<double> &rpm  = t.data[colRpm];
    const QVector<double> &time = t.data[colTime];

    // Threshold for "loaded": prefer load >= 85% of session max; fallback throttle >= 95.
    double loadThreshold = 0.0;
    bool   useLoad       = (colLoad >= 0);
    if (useLoad)       loadThreshold = 0.85 * maxOf(t.data[colLoad]);
    double throttleThreshold = 95.0;

    auto loaded = [&](int i) {
        if (useLoad) return t.data[colLoad][i] >= loadThreshold;
        if (colThrottle >= 0) return t.data[colThrottle][i] >= throttleThreshold;
        return true; // no info — accept
    };

    int segStart = -1;
    double segRpmMin = 0, segRpmMax = 0;
    for (int i = 1; i < t.rowCount(); ++i) {
        bool rising = rpm[i] >= rpm[i-1];
        bool ld     = loaded(i);
        if (rising && ld) {
            if (segStart < 0) {
                segStart  = i;
                segRpmMin = rpm[i];
                segRpmMax = rpm[i];
            }
            if (rpm[i] > segRpmMax) segRpmMax = rpm[i];
        } else {
            if (segStart >= 0) {
                double dRpm = segRpmMax - segRpmMin;
                double dt   = time[i] - time[segStart];
                if (dRpm >= 1500.0 && dt >= 1500.0) {
                    Pull p;
                    p.rowStart   = segStart;
                    p.rowEnd     = i - 1;
                    p.timeStartMs = time[p.rowStart];
                    p.timeEndMs   = time[p.rowEnd];
                    p.rpmStart   = segRpmMin;
                    p.rpmPeak    = segRpmMax;
                    if (colGear >= 0) {
                        // round-to-int gear at start
                        p.gearGuess = int(t.data[colGear][p.rowStart] + 0.5);
                    }
                    out.push_back(p);
                }
                segStart = -1;
            }
        }
    }
    // close trailing segment
    if (segStart >= 0) {
        double dRpm = segRpmMax - segRpmMin;
        double dt   = time.back() - time[segStart];
        if (dRpm >= 1500.0 && dt >= 1500.0) {
            Pull p;
            p.rowStart   = segStart;
            p.rowEnd     = t.rowCount() - 1;
            p.timeStartMs = time[p.rowStart];
            p.timeEndMs   = time[p.rowEnd];
            p.rpmStart   = segRpmMin;
            p.rpmPeak    = segRpmMax;
            if (colGear >= 0) p.gearGuess = int(t.data[colGear][p.rowStart] + 0.5);
            out.push_back(p);
        }
    }
    return out;
}

PullStats PullDetector::statsFor(const Pull &p, const LogTable &t, EcuFamily family)
{
    PullStats s; s.span = p;
    if (p.rowEnd < p.rowStart || p.rowEnd >= t.rowCount()) return s;

    int colBoost      = ChannelAlias::findColumn(t, family, Signal::IntakeManifoldPressure);
    int colTorque     = ChannelAlias::findColumn(t, family, Signal::EngineTorque);
    int colIat        = ChannelAlias::findColumn(t, family, Signal::IntakeAirTemp);
    int colLambda     = ChannelAlias::findColumn(t, family, Signal::LambdaActual);
    int colThrottle   = ChannelAlias::findColumn(t, family, Signal::ThrottlePosition);
    int colRpm        = ChannelAlias::findColumn(t, family, Signal::EngineRpm);

    // Per-cylinder knock retard: sum across cyls × time (Δt between samples).
    QVector<int> knockCols;
    for (int i = 0; i < t.colCount(); ++i) {
        AliasMatch m = ChannelAlias::resolve(t.columns[i].name, family);
        if (m.signal == Signal::KnockRetardCyl) knockCols.push_back(i);
    }

    double bestBoost = -std::numeric_limits<double>::infinity();
    double bestBoostRpm = 0;
    double afrAtBest = 0;
    double peakTorque = -std::numeric_limits<double>::infinity();
    double peakIat    = -std::numeric_limits<double>::infinity();
    double knockSum   = 0;
    double fullThrottleMs = 0;
    double prevT = (p.rowStart > 0) ? t.timeMs[p.rowStart - 1] : t.timeMs[p.rowStart];

    for (int i = p.rowStart; i <= p.rowEnd; ++i) {
        double dtMs = t.timeMs[i] - prevT; if (dtMs < 0) dtMs = 0;
        prevT = t.timeMs[i];

        if (colBoost >= 0) {
            double v = valueCanonical(t, colBoost, i);
            if (v > bestBoost) {
                bestBoost = v;
                if (colRpm >= 0) bestBoostRpm = t.data[colRpm][i];
                if (colLambda >= 0) afrAtBest = t.data[colLambda][i] * 14.7; // convert λ to AFR(petrol)
            }
        }
        if (colTorque >= 0) {
            double v = t.data[colTorque][i];
            if (v > peakTorque) peakTorque = v;
        }
        if (colIat >= 0) {
            double v = valueCanonical(t, colIat, i);
            if (v > peakIat) peakIat = v;
        }
        if (colThrottle >= 0) {
            // anything >= 95% counts
            if (t.data[colThrottle][i] >= 95.0) fullThrottleMs += dtMs;
        }
        for (int kc : knockCols) {
            // sum |retard| × dt; raw values are already in degrees
            double v = std::abs(t.data[kc][i]);
            knockSum += v * (dtMs / 1000.0); // °·s
        }
    }

    if (std::isfinite(bestBoost))  s.peakBoostBar       = bestBoost;
    if (std::isfinite(peakTorque)) s.peakTorqueNm       = peakTorque;
    if (std::isfinite(peakIat))    s.peakIntakeAirTempC = peakIat;
    s.afrAtPeakBoost     = afrAtBest;
    s.knockRetardSumDeg  = knockSum;
    s.timeAtFullThrottleMs = fullThrottleMs;
    s.rpmAtPeakBoost     = bestBoostRpm;
    return s;
}

QVector<PullStats> PullDetector::statsForAll(const QVector<Pull> &pulls, const LogTable &t, EcuFamily family)
{
    QVector<PullStats> out;
    out.reserve(pulls.size());
    for (const Pull &p : pulls) out.push_back(statsFor(p, t, family));
    return out;
}

} // namespace datalog
