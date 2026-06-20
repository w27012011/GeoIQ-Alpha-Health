#include "spatial/SpatialAnalysis.h"
#include "utils/MathUtils.h"
#include "utils/Statistics.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace GeoIQ {

    SpatialAnalysis::ConsistencyResult SpatialAnalysis::spatialConsistency(
        const SamplePoint&                     point,
        ElementField                           field,
        const std::vector<const SamplePoint*>& neighbours,
        double                                 z_threshold) const {
        (void)point;
        std::vector<double> local_log_vals;
        local_log_vals.reserve(neighbours.size());

        for (const auto* np : neighbours) {
            double raw = getFieldValue(*np, field);
            if (hasData(raw)) {
                double lv = Math::logTransform(raw, field);
                if (hasData(lv)) {
                    local_log_vals.push_back(lv);
                }
            }
        }

        WindowStats stats = Stats::computeWindowStats(local_log_vals);

        if (!stats.valid) {
            return ConsistencyResult{
                0.0, 0, static_cast<int>(neighbours.size()),
                "Insufficient neighbours for consistency check"
            };
        }

        int n_agreeing = 0;
        for (const auto* np : neighbours) {
            double raw_np = getFieldValue(*np, field);
            if (!hasData(raw_np)) {
                continue;
            }
            double z_np = Math::computeZScore(raw_np, stats.median, stats.stddev, field);
            if (hasData(z_np) && z_np >= z_threshold) {
                n_agreeing++;
            }
        }

        int n_total = static_cast<int>(neighbours.size());
        double consistency = (n_total > 0) ? static_cast<double>(n_agreeing) / n_total : 0.0;

        std::stringstream ss;
        ss << n_agreeing << "/" << n_total << " neighbours anomalous → ";
        if (consistency >= 0.70) {
            ss << "strong spatial coherence";
        } else if (consistency >= 0.40) {
            ss << "moderate spatial coherence";
        } else {
            ss << "isolated anomaly, treat with caution";
        }

        return ConsistencyResult{consistency, n_agreeing, n_total, ss.str()};
    }

    SpatialAnalysis::DispersionResult SpatialAnalysis::detectDispersionTrain(
        const SamplePoint& point,
        ElementField       field,
        const Dataset&     dataset,
        double             search_radius_km) const {

        // Check medium type
        if (point.sample_type == SampleType::BOREHOLE || point.sample_type == SampleType::GROUNDWATER) {
            return DispersionResult{false, NO_DATA, NO_DATA, 0.0, "Borehole/Groundwater medium: 2D dispersion model not applicable"};
        }

        // Query neighbours within search radius
        std::vector<const SamplePoint*> nbrs = dataset.queryRadius(point.lat, point.lon, search_radius_km);
        if (nbrs.size() < 4) {
            return DispersionResult{false, NO_DATA, NO_DATA, 0.0, "Insufficient data for train detection"};
        }

        // Check if query point itself is anomalous
        double local_r_used = DEFAULT_LOCAL_RADIUS_KM;
        std::vector<const SamplePoint*> local_nbrs = dataset.queryRadiusAdaptive(
            point.lat, point.lon, DEFAULT_LOCAL_RADIUS_KM, MIN_NEIGHBOURS_FOR_STATS, local_r_used);

        std::vector<double> local_log_vals;
        local_log_vals.reserve(local_nbrs.size());
        for (const auto* np : local_nbrs) {
            double raw = getFieldValue(*np, field);
            if (hasData(raw)) {
                double lv = Math::logTransform(raw, field);
                if (hasData(lv)) {
                    local_log_vals.push_back(lv);
                }
            }
        }

        WindowStats stats = Stats::computeWindowStats(local_log_vals);
        if (!stats.valid) {
            return DispersionResult{false, NO_DATA, NO_DATA, 0.0, "Insufficient local neighbours to compute Z-score"};
        }

        double point_raw = getFieldValue(point, field);
        double local_z = Math::computeZScore(point_raw, stats.median, stats.stddev, field);
        if (!hasData(local_z) || local_z < ANOMALY_Z_THRESHOLD) {
            return DispersionResult{false, NO_DATA, NO_DATA, 0.0, "Point not anomalous"};
        }

        // Divide compass into 8 sectors
        double bearings[] = {0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0};
        double sector_means[8];
        int valid_sectors_count = 0;

        for (int i = 0; i < 8; ++i) {
            sector_means[i] = sectorMean(point, field, nbrs, bearings[i], 45.0);
            if (hasData(sector_means[i])) {
                valid_sectors_count++;
            }
        }

        if (valid_sectors_count < 3) {
            return DispersionResult{false, NO_DATA, NO_DATA, 0.0, "Insufficient directional coverage (fewer than 3 valid sectors)"};
        }

        double max_mean = -1.0;
        double max_sector_bearing = NO_DATA;
        double min_mean = std::numeric_limits<double>::max();

        for (int i = 0; i < 8; ++i) {
            if (hasData(sector_means[i])) {
                if (sector_means[i] > max_mean) {
                    max_mean = sector_means[i];
                    max_sector_bearing = bearings[i];
                }
                if (sector_means[i] < min_mean) {
                    min_mean = sector_means[i];
                }
            }
        }

        double gradient_strength = (max_mean - min_mean) / (max_mean + 1e-9);
        gradient_strength = std::clamp(gradient_strength, 0.0, 1.0);

        if (gradient_strength < 0.20) {
            return DispersionResult{false, NO_DATA, NO_DATA, gradient_strength, "No clear gradient detected"};
        }

        // Estimate source distance using exponential decay
        double lambda = getDecayLambda(field);
        double source_distance_km = search_radius_km * 0.5; // fallback estimate
        if (max_mean > point_raw && point_raw > 0.0) {
            double ratio = max_mean / point_raw;
            source_distance_km = (1.0 / lambda) * std::log(ratio);
            source_distance_km = std::clamp(source_distance_km, 0.5, search_radius_km);
        }

        std::stringstream ss;
        ss << "Concentration gradient detected toward " << bearingName(max_sector_bearing) << ". "
           << "Probable source ~" << std::fixed << std::setprecision(1) << source_distance_km << "km " << bearingName(max_sector_bearing) << ". "
           << "Gradient strength: " << std::fixed << std::setprecision(2) << gradient_strength << ". "
           << "Lambda decay: " << lambda << "/km (" << elementFieldToString(field) << " mobility class)";

        return DispersionResult{true, max_sector_bearing, source_distance_km, gradient_strength, ss.str()};
    }

    double SpatialAnalysis::sectorMean(
        const SamplePoint&                      query,
        ElementField                            field,
        const std::vector<const SamplePoint*>&  neighbours,
        double bearing_centre_deg,
        double sector_width_deg) const {

        std::vector<double> in_sector_vals;
        in_sector_vals.reserve(neighbours.size());

        for (const auto* np : neighbours) {
            if (inSector(query, *np, bearing_centre_deg, sector_width_deg)) {
                double raw = getFieldValue(*np, field);
                if (hasData(raw) && raw > 0.0) {
                    in_sector_vals.push_back(raw);
                }
            }
        }

        if (in_sector_vals.empty()) {
            return NO_DATA;
        }

        double sum = 0.0;
        for (double v : in_sector_vals) {
            sum += v;
        }
        return sum / in_sector_vals.size();
    }

    bool SpatialAnalysis::inSector(
        const SamplePoint& query,
        const SamplePoint& neighbour,
        double bearing_centre_deg,
        double sector_width_deg) const {

        double bearing = Math::bearingDegrees(query.lat, query.lon, neighbour.lat, neighbour.lon);
        double diff = std::abs(bearing - bearing_centre_deg);
        if (diff > 180.0) {
            diff = 360.0 - diff;
        }
        return diff <= sector_width_deg / 2.0;
    }

    double SpatialAnalysis::getDecayLambda(ElementField field) {
        switch (field) {
            case ElementField::Au: return 0.05; // Very immobile — stays close to source
            case ElementField::Pb: return 0.08; // Immobile in near-neutral conditions
            case ElementField::Ba: return 0.03; // Barite nearly insoluble — very close to source
            case ElementField::Bi:
            case ElementField::Sb:
            case ElementField::Te: return 0.10; // Moderately immobile
            case ElementField::As:
            case ElementField::Cu:
            case ElementField::Co:
            case ElementField::Ni:
            case ElementField::Hg:
            case ElementField::Tl: return 0.15; // Moderate mobility
            case ElementField::Zn:
            case ElementField::Cd: return 0.25; // Mobile — can travel far from source
            case ElementField::Mn: return 0.20; // Mobile in oxidising conditions
            default:               return 0.15; // Default moderate
        }
    }

    std::string SpatialAnalysis::bearingName(double degrees) {
        if (degrees < 0.0) {
            degrees = std::fmod(degrees + 360.0, 360.0);
        }
        if (degrees >= 337.5 || degrees < 22.5)   return "N";
        if (degrees >= 22.5 && degrees < 67.5)    return "NE";
        if (degrees >= 67.5 && degrees < 112.5)   return "E";
        if (degrees >= 112.5 && degrees < 157.5)  return "SE";
        if (degrees >= 157.5 && degrees < 202.5)  return "S";
        if (degrees >= 202.5 && degrees < 247.5)  return "SW";
        if (degrees >= 247.5 && degrees < 292.5)  return "W";
        return "NW";
    }
}
