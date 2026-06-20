#ifndef GEOIQ_MATHUTILS_H
#define GEOIQ_MATHUTILS_H

#include <vector>
#include <string>
#include <cmath>
#include "utils/Constants.h"
#include "utils/Enums.h"

namespace GeoIQ {
    namespace Math {

        // Epsilon tolerance check for missing data sentinels (TRAP-02)
        inline bool isNoData(double v) {
            return std::abs(v - NO_DATA) < 1e-3;
        }

        // Helper to check for valid coordinates or element data presence
        inline bool hasData(double v) {
            return !isNoData(v);
        }

        // Calculates Haversine distance in km between two decimal degrees coordinates
        double haversine(double lat1, double lon1, double lat2, double lon2);

        // Standardized log10 transform for trace elements and oxides (CONTRACT-07)
        double logTransform(double value, ElementField field);

        // Computes Z-score in log space (CONTRACT-04)
        double computeZScore(double point_value, double median_log, double stddev_log, ElementField field);

        // Combines local and regional Z-scores into a weighted score
        double computeCombinedScore(double local_z, double regional_z);

        // Ordinary Least Squares (OLS) linear regression y = ax + b (Cheng 2007)
        bool linearRegression(const std::vector<double>& x_vals, const std::vector<double>& y_vals,
                              double& slope_out, double& intercept_out, double& r_squared_out);

        // Compiles the JIT Z-score formula string representation for evidence audit trails
        std::string formulaString(const std::string& element, double raw_value, double log_value,
                                  double median_log, double stddev_log, double z_score);

        // Compass bearing in degrees (0 - 360) from point 1 to point 2
        double bearingDegrees(double lat1, double lon1, double lat2, double lon2);
    }
}

#endif // GEOIQ_MATHUTILS_H
