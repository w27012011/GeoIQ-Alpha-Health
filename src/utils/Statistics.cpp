#include "utils/Statistics.h"
#include "utils/MathUtils.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace GeoIQ {
    namespace Stats {

        WindowStats computeWindowStats(const std::vector<double>& log_values) {
            std::vector<double> clean_vals;
            clean_vals.reserve(log_values.size());

            // TRAP-03: Filter out NaN, infinity, and NO_DATA values before sorting
            for (double val : log_values) {
                if (std::isfinite(val) && !Math::isNoData(val)) {
                    clean_vals.push_back(val);
                }
            }

            int n = static_cast<int>(clean_vals.size());
            if (n < MIN_NEIGHBOURS_FOR_STATS) {
                WindowStats invalid_stats;
                invalid_stats.n = n;
                invalid_stats.valid = false;
                return invalid_stats;
            }

            // Perform sorting on filtered data (safe strict weak ordering contract)
            std::sort(clean_vals.begin(), clean_vals.end());

            WindowStats stats;
            stats.n = n;
            stats.valid = true;

            // Median calculation
            if (n % 2 != 0) {
                stats.median = clean_vals[n / 2];
            } else {
                stats.median = (clean_vals[n / 2 - 1] + clean_vals[n / 2]) / 2.0;
            }

            // Mean calculation
            double sum = std::accumulate(clean_vals.begin(), clean_vals.end(), 0.0);
            stats.mean = sum / n;

            // Sample standard deviation: s = sqrt(sum((xi - mean)^2) / (n - 1))
            double variance_sum = 0.0;
            for (double val : clean_vals) {
                double diff = val - stats.mean;
                variance_sum += diff * diff;
            }
            stats.stddev = std::sqrt(variance_sum / (n - 1));

            // Standard deviation floor guard to prevent division-by-zero Z-score explosion
            if (stats.stddev < 0.05) {
                stats.stddev = 0.05;
            }

            // Percentile calculations
            stats.p05 = clean_vals[std::clamp(static_cast<int>(std::floor(0.05 * n)), 0, n - 1)];
            stats.p95 = clean_vals[std::clamp(static_cast<int>(std::floor(0.95 * n)), 0, n - 1)];

            return stats;
        }

        double geometricMean(const std::vector<double>& values) {
            std::vector<double> log_vals;
            log_vals.reserve(values.size());

            for (double v : values) {
                if (Math::hasData(v) && v > 0.0) {
                    log_vals.push_back(std::log10(v + LOG_ZERO_OFFSET));
                }
            }

            if (log_vals.empty()) {
                return NO_DATA;
            }

            double sum_log = std::accumulate(log_vals.begin(), log_vals.end(), 0.0);
            double mean_log = sum_log / log_vals.size();
            return std::pow(10.0, mean_log);
        }

        double confidenceScore(int n_local) {
            if (n_local < MIN_NEIGHBOURS_FOR_STATS) {
                return 0.0;
            }
            return std::min(1.0, static_cast<double>(n_local) / TARGET_NEIGHBOURS);
        }

        RiskLevel riskLevelFromScore(double score) {
            // Epsilon check for missing score sentinels (TRAP-02)
            if (Math::isNoData(score)) {
                return RiskLevel::INSUFFICIENT_DATA;
            }

            if (score < 0.30)  return RiskLevel::LOW;
            if (score < 0.50)  return RiskLevel::MEDIUM;
            if (score < 0.65)  return RiskLevel::MEDIUM_HIGH;
            if (score < 0.80)  return RiskLevel::HIGH;
            return RiskLevel::CRITICAL;
        }

        std::string riskLevelToString(RiskLevel rl) {
            switch (rl) {
                case RiskLevel::INSUFFICIENT_DATA: return "INSUFFICIENT_DATA";
                case RiskLevel::LOW:               return "LOW";
                case RiskLevel::MEDIUM:            return "MEDIUM";
                case RiskLevel::MEDIUM_HIGH:       return "MEDIUM_HIGH";
                case RiskLevel::HIGH:              return "HIGH";
                case RiskLevel::CRITICAL:          return "CRITICAL";
                default:                           return "UNKNOWN";
            }
        }

        std::string targetToString(Target t) {
            switch (t) {
                case Target::ARSENIC_HAZARD:     return "ARSENIC_HAZARD";
                case Target::FLUORIDE_HAZARD:    return "FLUORIDE_HAZARD";
                case Target::GOLD:               return "GOLD";
                case Target::GOSSAN:             return "GOSSAN";
                case Target::DIAMOND_KIMBERLITE: return "DIAMOND_KIMBERLITE";
                case Target::COPPER_PORPHYRY:    return "COPPER_PORPHYRY";
                case Target::COPPER_VMS:         return "COPPER_VMS";
                case Target::LITHIUM_PEGMATITE:  return "LITHIUM_PEGMATITE";
                case Target::LITHIUM_BRINE:     return "LITHIUM_BRINE";
                case Target::REE_CARBONATITE:    return "REE_CARBONATITE";
                case Target::NICKEL_PGE:         return "NICKEL_PGE";
                case Target::ZINC_LEAD:          return "ZINC_LEAD";
                default:                         return "UNKNOWN";
            }
        }

        std::string singularityStateToString(SingularityState ss) {
            switch (ss) {
                case SingularityState::ENRICHED:   return "ENRICHED";
                case SingularityState::BACKGROUND: return "BACKGROUND";
                case SingularityState::DEPLETED:   return "DEPLETED";
                case SingularityState::INVALID:    return "INVALID";
                default:                           return "UNKNOWN";
            }
        }
    }
}
