#pragma once
#include "CanonicalSignal.h"
#include "EcuFamily.h"
#include "LogTable.h"

#include <QString>
#include <QHash>

namespace datalog {

struct AliasMatch {
    Signal  signal;
    int     cylIndex;   // -1 for non-array signals; 1..N for KnockRetardCyl etc.
};

// Resolves a vendor channel name to a canonical Signal, given the detected ECU family.
// Falls back to Signal::Unknown when no match.
class ChannelAlias {
public:
    static AliasMatch resolve(const QString &vendorName, EcuFamily family);

    // Convenience: build a map column-index -> AliasMatch for the whole LogTable.
    static QHash<int, AliasMatch> resolveAll(const LogTable &t, EcuFamily family);

    // Find the column index for a given canonical signal (first match wins).
    // Returns -1 if not present.
    static int findColumn(const LogTable &t, EcuFamily family, Signal s, int cylIndex = -1);
};

} // namespace datalog
