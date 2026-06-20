#include "data/Dataset.h"
#include "utils/MathUtils.h"
#include "utils/Logger.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace GeoIQ {

    Dataset::Dataset(std::vector<SamplePoint> points)
        : points_(std::move(points)) {
        computeAllRatios();
        kdtree_.build(points_);
        buildBoundingBox();

        std::stringstream ss;
        ss << "Dataset built: " << points_.size() << " points. Bbox: ["
           << std::fixed << std::setprecision(6)
           << min_lat_ << "," << max_lat_ << "] x ["
           << min_lon_ << "," << max_lon_ << "]";
        Logger::getInstance().info("Dataset", ss.str());
    }

    std::vector<const SamplePoint*> Dataset::queryRadius(double lat, double lon, double radius_km) const {
        std::vector<const SamplePoint*> raw_results = kdtree_.radiusSearch(lat, lon, radius_km);
        std::vector<const SamplePoint*> filtered;
        filtered.reserve(raw_results.size());
        for (const auto* p : raw_results) {
            // Exclude the exact query point
            if (std::abs(p->lat - lat) < 1e-9 && std::abs(p->lon - lon) < 1e-9) {
                continue;
            }
            filtered.push_back(p);
        }
        return filtered;
    }

    std::vector<const SamplePoint*> Dataset::queryRadiusAdaptive(
        double lat, double lon,
        double radius_km,
        int    min_count,
        double& radius_used_out) const {
        
        double radius = radius_km;
        std::vector<const SamplePoint*> results;
        while (true) {
            results = queryRadius(lat, lon, radius);
            if (static_cast<int>(results.size()) >= min_count) {
                break;
            }
            if (radius >= radius_km * MAX_RADIUS_EXPAND_FACTOR) {
                break;
            }
            radius = radius * 1.5; // Expand by 50% each iteration
        }

        if (radius > radius_km) {
            // Suppress per-call WARN: 600K+ file writes/run were the primary runtime bottleneck
            // (CPU sat at 1-2% waiting on I/O syscalls). The aggregate count is reported
            // correctly in report.txt as "Radius Expansion Events". No scientific info is lost.
            expanded_radius_count_++;
        }
        radius_used_out = radius;
        return results;
    }

    std::vector<const SamplePoint*> Dataset::queryKNearest(double lat, double lon, int k) const {
        // Request k + 1 points to account for potentially filtering the query point itself
        std::vector<const SamplePoint*> raw_results = kdtree_.knnSearch(lat, lon, k + 1);
        std::vector<const SamplePoint*> filtered;
        filtered.reserve(k);
        for (const auto* p : raw_results) {
            if (std::abs(p->lat - lat) < 1e-9 && std::abs(p->lon - lon) < 1e-9) {
                continue;
            }
            filtered.push_back(p);
            if (static_cast<int>(filtered.size()) == k) {
                break;
            }
        }
        return filtered;
    }

    const std::vector<SamplePoint>& Dataset::getPoints() const {
        return points_;
    }

    int Dataset::size() const {
        return static_cast<int>(points_.size());
    }

    int Dataset::getExpandedRadiusCount() const {
        return expanded_radius_count_;
    }

    double Dataset::minLat() const { return min_lat_; }
    double Dataset::maxLat() const { return max_lat_; }
    double Dataset::minLon() const { return min_lon_; }
    double Dataset::maxLon() const { return max_lon_; }

    void Dataset::computeAllRatios() {
        for (auto& p : points_) {
            applyDerivedRatios(p);
        }
    }

    void Dataset::buildBoundingBox() {
        min_lat_ = 90.0;
        max_lat_ = -90.0;
        min_lon_ = 180.0;
        max_lon_ = -180.0;
        for (const auto& p : points_) {
            if (hasData(p.lat) && hasData(p.lon)) {
                if (p.lat < min_lat_) min_lat_ = p.lat;
                if (p.lat > max_lat_) max_lat_ = p.lat;
                if (p.lon < min_lon_) min_lon_ = p.lon;
                if (p.lon > max_lon_) max_lon_ = p.lon;
            }
        }
    }

    void Dataset::applyDerivedRatios(SamplePoint& p) {
        // K_Rb_ratio:
        if (hasData(p.K2O) && hasData(p.Rb) && p.Rb > 0.0) {
            double K_ppm = p.K2O * 0.8302 * 10000.0; // K2O% -> K% -> K ppm
            p.K_Rb_ratio = K_ppm / p.Rb;
        } else {
            p.K_Rb_ratio = NO_DATA;
        }

        // Nb_Ta_ratio:
        if (hasData(p.Nb) && hasData(p.Ta) && p.Ta > 0.0) {
            p.Nb_Ta_ratio = p.Nb / p.Ta;
        } else {
            p.Nb_Ta_ratio = NO_DATA;
        }

        // Rb_Sr_ratio:
        if (hasData(p.Rb) && hasData(p.Sr) && p.Sr > 0.0) {
            p.Rb_Sr_ratio = p.Rb / p.Sr;
        } else {
            p.Rb_Sr_ratio = NO_DATA;
        }

        // Cr_Ni_ratio:
        if (hasData(p.Cr) && hasData(p.Ni) && p.Ni > 0.0) {
            p.Cr_Ni_ratio = p.Cr / p.Ni;
        } else {
            p.Cr_Ni_ratio = NO_DATA;
        }

        // Zn_Pb_ratio:
        if (hasData(p.Zn) && hasData(p.Pb) && (p.Zn + p.Pb) > 0.0) {
            p.Zn_Pb_ratio = p.Zn / (p.Zn + p.Pb);
        } else {
            p.Zn_Pb_ratio = NO_DATA;
        }

        // Cu_Mo_ratio:
        if (hasData(p.Cu) && hasData(p.Mo) && p.Mo > 0.0) {
            p.Cu_Mo_ratio = p.Cu / p.Mo;
        } else {
            p.Cu_Mo_ratio = NO_DATA;
        }

        // Mg_Li_ratio:
        if (hasData(p.MgO) && hasData(p.Li) && p.Li > 0.0) {
            p.Mg_Li_ratio = p.MgO / p.Li;
        } else {
            p.Mg_Li_ratio = NO_DATA;
        }
    }
}
