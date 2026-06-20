#include "utils/MathUtils.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace GeoIQ {
    namespace Math {

        double haversine(double lat1, double lon1, double lat2, double lon2) {
            double phi1 = lat1 * PI / 180.0;
            double phi2 = lat2 * PI / 180.0;
            double dphi = (lat2 - lat1) * PI / 180.0;
            double dlam = (lon2 - lon1) * PI / 180.0;

            double a = sin(dphi / 2.0) * sin(dphi / 2.0) +
                       cos(phi1) * cos(phi2) * sin(dlam / 2.0) * sin(dlam / 2.0);
            
            // Precision safety clamp to prevent NaN inside sqrt
            a = std::max(0.0, std::min(1.0, a));
            
            double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
            return EARTH_RADIUS_KM * c;
        }

        double logTransform(double value, ElementField field) {
            if (isNoData(value) || value < 0.0) {
                return NO_DATA;
            }

            if (field == ElementField::pH || field == ElementField::Eh) {
                return value; // Already logarithmic or linear redox potential
            }

            int idx = static_cast<int>(field);
            // Check if field is a major oxide (Fe to LOI)
            if (idx >= static_cast<int>(ElementField::Fe) && idx <= static_cast<int>(ElementField::LOI)) {
                if (value < 0.01) {
                    return log10(value + LOG_ZERO_OFFSET);
                }
                return value; // No log-transform for major % oxides >= 0.01%
            }

            return log10(value + LOG_ZERO_OFFSET);
        }

        double computeZScore(double point_value, double median_log, double stddev_log, ElementField field) {
            double point_log = logTransform(point_value, field);
            
            if (isNoData(point_log) || isNoData(median_log) || isNoData(stddev_log)) {
                return NO_DATA;
            }

            // Floor check to prevent division-by-zero Z-score explosion
            double effective_stddev = (stddev_log < 0.05) ? 0.05 : stddev_log;

            double raw_z = (point_log - median_log) / effective_stddev;
            return std::clamp(raw_z, -12.0, 12.0);
        }

        double computeCombinedScore(double local_z, double regional_z) {
            bool has_local = hasData(local_z);
            bool has_regional = hasData(regional_z);

            if (has_local && has_regional) {
                return LOCAL_Z_WEIGHT * local_z + REGIONAL_Z_WEIGHT * regional_z;
            } else if (has_local) {
                return local_z;
            } else if (has_regional) {
                return regional_z;
            }
            return NO_DATA;
        }

        bool linearRegression(const std::vector<double>& x_vals, const std::vector<double>& y_vals,
                              double& slope_out, double& intercept_out, double& r_squared_out) {
            size_t n = x_vals.size();
            if (n < 2 || n != y_vals.size()) {
                return false;
            }

            double sum_x = 0.0, sum_y = 0.0;
            for (size_t i = 0; i < n; ++i) {
                sum_x += x_vals[i];
                sum_y += y_vals[i];
            }

            double x_mean = sum_x / n;
            double y_mean = sum_y / n;

            double ssxx = 0.0, ssyy = 0.0, ssxy = 0.0;
            for (size_t i = 0; i < n; ++i) {
                double dx = x_vals[i] - x_mean;
                double dy = y_vals[i] - y_mean;
                ssxx += dx * dx;
                ssyy += dy * dy;
                ssxy += dx * dy;
            }

            // Zero-variance denominator safety guard (TRAP-07)
            if (std::abs(ssxx) < 1e-10) {
                return false;
            }

            slope_out = ssxy / ssxx;
            intercept_out = y_mean - slope_out * x_mean;

            if (std::abs(ssyy) < 1e-12) {
                r_squared_out = 1.0; // Perfect flat fit
            } else {
                double r = ssxy / (sqrt(ssxx) * sqrt(ssyy));
                r_squared_out = std::max(0.0, std::min(1.0, r * r));
            }

            return true;
        }

        std::string formulaString(const std::string& element, double raw_value, double log_value,
                                  double median_log, double stddev_log, double z_score) {
            (void)log_value;
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(3);

            if (element == "pH") {
                ss << "Z=(" << std::setprecision(2) << raw_value << "-" << median_log << ")/" << stddev_log << "=" << std::setprecision(3) << z_score;
                return ss.str();
            }

            // Check if element is a major oxide
            // (We can pass the raw name string here for lazy formatting)
            bool is_oxide = (element == "Fe" || element == "Al" || element == "SiO2" ||
                             element == "MgO" || element == "K2O" || element == "Na2O" ||
                             element == "CaO" || element == "P2O5" || element == "S" || element == "LOI");

            if (is_oxide && raw_value >= 0.01) {
                ss << "Z=(" << raw_value << "-" << median_log << ")/" << stddev_log << "=" << z_score;
            } else {
                ss << "Z=(log10(" << raw_value << "+0.001)-" << median_log << ")/" << stddev_log << "=" << z_score;
            }

            return ss.str();
        }

        double bearingDegrees(double lat1, double lon1, double lat2, double lon2) {
            double phi1 = lat1 * PI / 180.0;
            double phi2 = lat2 * PI / 180.0;
            double dlam = (lon2 - lon1) * PI / 180.0;

            double y = sin(dlam) * cos(phi2);
            double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dlam);
            
            double bearing = atan2(y, x) * 180.0 / PI;
            return fmod(bearing + 360.0, 360.0);
        }
    }
}
