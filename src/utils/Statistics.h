#ifndef GEOIQ_STATISTICS_H
#define GEOIQ_STATISTICS_H

#include <vector>
#include <string>
#include "utils/Enums.h"
#include "utils/Structs.h"

namespace GeoIQ {
    namespace Stats {

        // Computes WindowStats (median, mean, stddev, percentiles) for log10 values.
        // Performs strict sorting contract checks to eliminate NaN UB (TRAP-03)
        WindowStats computeWindowStats(const std::vector<double>& log_values);

        // Computes geometric mean of raw concentrations (Cheng 2007)
        double geometricMean(const std::vector<double>& values);

        // Computes confidence index based on local sample density
        double confidenceScore(int n_local);

        // Maps normalized anomaly score to RiskLevel
        RiskLevel riskLevelFromScore(double score);

        // Enum serialization string lookup utilities
        std::string riskLevelToString(RiskLevel rl);
        std::string targetToString(Target t);
        std::string singularityStateToString(SingularityState ss);
    }
}

#endif // GEOIQ_STATISTICS_H
