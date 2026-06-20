#include "spatial/IDW.h"
#include "utils/MathUtils.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace GeoIQ {
    namespace IDW {

        double interpolate(
            double query_lat, double query_lon,
            const std::vector<const SamplePoint*>& neighbours,
            ElementField field,
            double power) {

            // 1. Check for exact matches (distance < 1e-6)
            double exact_sum = 0.0;
            int exact_count = 0;
            bool has_exact_location = false;

            for (const auto* np : neighbours) {
                double dist = Math::haversine(query_lat, query_lon, np->lat, np->lon);
                if (dist < 1e-6) {
                    has_exact_location = true;
                    double raw_val = getFieldValue(*np, field);
                    if (hasData(raw_val)) {
                        exact_sum += raw_val;
                        exact_count++;
                    }
                } else {
                    // Since neighbors are sorted by distance ascending, once we see dist >= 1e-6,
                    // we can stop checking for exact matches.
                    break;
                }
            }

            if (has_exact_location) {
                if (exact_count > 0) {
                    return exact_sum / exact_count;
                } else {
                    return NO_DATA;
                }
            }

            // 2. Perform IDW interpolation
            double sum_w = 0.0;
            double sum_wv = 0.0;

            for (const auto* np : neighbours) {
                double raw_val = getFieldValue(*np, field);
                if (!hasData(raw_val)) {
                    continue;
                }
                double dist = Math::haversine(query_lat, query_lon, np->lat, np->lon);
                // Safe guard against zero/negative distances (though we already checked dist < 1e-6)
                if (dist < 1e-6) {
                    continue; 
                }
                double w = 1.0 / std::pow(dist, power);
                sum_w += w;
                sum_wv += w * raw_val;
            }

            if (sum_w < 1e-12) {
                return NO_DATA;
            }

            return sum_wv / sum_w;
        }

        SamplePoint interpolateAllElements(
            double query_lat, double query_lon,
            const std::vector<const SamplePoint*>& neighbours,
            double power) {

            SamplePoint p;
            p.lat = query_lat;
            p.lon = query_lon;
            p.depth_m = NO_DATA;
            p.sample_type = SampleType::UNKNOWN;
            p.horizon = HorizonType::UNKNOWN;
            p.source = "INTERPOLATED";

            std::stringstream ss;
            ss << "GRID_" << std::fixed << std::setprecision(4) << query_lat << "_" << query_lon;
            p.sample_id = ss.str();

            for (int field_idx = 0; field_idx < static_cast<int>(ElementField::COUNT); ++field_idx) {
                ElementField field = static_cast<ElementField>(field_idx);
                double val = interpolate(query_lat, query_lon, neighbours, field, power);
                setFieldValue(p, field, val);
            }

            return p;
        }
    }
}
