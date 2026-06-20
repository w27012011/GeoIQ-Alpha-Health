#include "spatial/MovingWindow.h"
#include "utils/MathUtils.h"
#include "utils/Statistics.h"
#include "utils/Logger.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace GeoIQ {

    MovingWindow::Result MovingWindow::compute(
        const SamplePoint& point,
        ElementField       field,
        const Dataset&     dataset) const {

        double local_r_used = local_radius_km;
        double regional_r_used = regional_radius_km;

        std::vector<const SamplePoint*> local_nbrs = dataset.queryRadiusAdaptive(
            point.lat, point.lon, local_radius_km, MIN_NEIGHBOURS_FOR_STATS, local_r_used);

        std::vector<const SamplePoint*> regional_nbrs = dataset.queryRadiusAdaptive(
            point.lat, point.lon, regional_radius_km, MIN_NEIGHBOURS_FOR_STATS, regional_r_used);

        SpatialContext ctx = computeContext(point, field, local_nbrs, regional_nbrs, local_r_used, regional_r_used);
        SingularityResult sing = computeSingularity(point, field, dataset);
        ElementEvidence ev = buildEvidence(point, field, ctx, sing);

        // LOG DEBUG per element per point
        std::stringstream ss;
        ss << point.sample_id << " " << elementFieldToString(field) << ": local_z=";
        if (hasData(ctx.local_z)) {
            ss << std::fixed << std::setprecision(2) << ctx.local_z;
        } else {
            ss << "NO_DATA";
        }
        ss << " alpha=";
        if (hasData(sing.alpha)) {
            ss << std::fixed << std::setprecision(2) << sing.alpha;
        } else {
            ss << "NO_DATA";
        }
        ss << " " << Stats::singularityStateToString(sing.state);
        if (point.sample_type == SampleType::VIRTUAL_GRID) {
            ss << " [VIRTUAL_GRID]";
        }
        Logger::getInstance().debug("MovingWindow", ss.str());

        return Result{ctx, sing, ev};
    }

    std::array<MovingWindow::Result, static_cast<size_t>(ElementField::COUNT)> MovingWindow::computeAll(
        const SamplePoint&              point,
        const Dataset&                  dataset) const {

        std::array<Result, static_cast<size_t>(ElementField::COUNT)> results;
        for (int i = 0; i < static_cast<int>(ElementField::COUNT); ++i) {
            ElementField field = static_cast<ElementField>(i);
            results[i] = compute(point, field, dataset);
        }
        return results;
    }

    SpatialContext MovingWindow::computeContext(
        const SamplePoint&                  point,
        ElementField                        field,
        const std::vector<const SamplePoint*>& local_nbrs,
        const std::vector<const SamplePoint*>& regional_nbrs,
        double local_radius_used,
        double regional_radius_used) const {

        // Extract log-transformed values from local neighbours
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

        // Extract log-transformed values from regional neighbours
        std::vector<double> regional_log_vals;
        regional_log_vals.reserve(regional_nbrs.size());
        for (const auto* np : regional_nbrs) {
            double raw = getFieldValue(*np, field);
            if (hasData(raw)) {
                double lv = Math::logTransform(raw, field);
                if (hasData(lv)) {
                    regional_log_vals.push_back(lv);
                }
            }
        }

        // Compute statistics
        WindowStats local_stats = Stats::computeWindowStats(local_log_vals);
        WindowStats regional_stats = Stats::computeWindowStats(regional_log_vals);

        // Compute Z-scores
        double point_raw = getFieldValue(point, field);
        double local_z = NO_DATA;
        double regional_z = NO_DATA;

        if (hasData(point_raw)) {
            if (local_stats.valid) {
                local_z = Math::computeZScore(point_raw, local_stats.median, local_stats.stddev, field);
            }
            if (regional_stats.valid) {
                regional_z = Math::computeZScore(point_raw, regional_stats.median, regional_stats.stddev, field);
            }
        }

        // Compute combined score and confidence
        double anomaly_score = Math::computeCombinedScore(local_z, regional_z);
        double confidence = Stats::confidenceScore(local_stats.n);

        SpatialContext ctx;
        ctx.local = local_stats;
        ctx.regional = regional_stats;
        ctx.local_z = local_z;
        ctx.regional_z = regional_z;
        ctx.anomaly_score = anomaly_score;
        ctx.confidence = confidence;
        ctx.local_radius_used = local_radius_used;
        ctx.regional_radius_used = regional_radius_used;

        return ctx;
    }

    SingularityResult MovingWindow::computeSingularity(
        const SamplePoint& point,
        ElementField       field,
        const Dataset&     dataset) const {

        std::vector<double> log_r_vals;
        std::vector<double> log_c_vals;
        log_r_vals.reserve(singularity_radii.size());
        log_c_vals.reserve(singularity_radii.size());

        for (double r_km : singularity_radii) {
            std::vector<const SamplePoint*> nbrs = dataset.queryRadius(point.lat, point.lon, r_km);

            std::vector<double> raw_vals;
            raw_vals.reserve(nbrs.size());
            for (const auto* np : nbrs) {
                double v = getFieldValue(*np, field);
                if (hasData(v) && v > 0.0) {
                    raw_vals.push_back(v);
                }
            }

            if (raw_vals.size() < 2) {
                continue; // Not enough data at this scale
            }

            double C_r = Stats::geometricMean(raw_vals);
            if (!hasData(C_r) || C_r <= 0.0) {
                continue;
            }

            log_r_vals.push_back(std::log10(r_km));
            log_c_vals.push_back(std::log10(C_r));
        }

        if (log_r_vals.size() < 3) {
            return SingularityResult{NO_DATA, 0.0, SingularityState::INVALID, false};
        }

        double min_log_r = *std::min_element(log_r_vals.begin(), log_r_vals.end());
        double max_log_r = *std::max_element(log_r_vals.begin(), log_r_vals.end());
        double log_r_span = max_log_r - min_log_r;

        if (log_r_span < 0.3) {
            return SingularityResult{NO_DATA, 0.0, SingularityState::INVALID, false};
        }

        double slope = 0.0, intercept = 0.0, r_sq = 0.0;
        bool ok = Math::linearRegression(log_r_vals, log_c_vals, slope, intercept, r_sq);
        if (!ok) {
            return SingularityResult{NO_DATA, 0.0, SingularityState::INVALID, false};
        }

        double alpha = slope + 2.0;

        SingularityState state = SingularityState::BACKGROUND;
        if (alpha < SINGULARITY_ENRICHED_MAX) {
            state = SingularityState::ENRICHED;
        } else if (alpha > SINGULARITY_DEPLETED_MIN) {
            state = SingularityState::DEPLETED;
        }

        bool valid = (r_sq >= SINGULARITY_MIN_R_SQUARED);
        if (!valid) {
            state = SingularityState::INVALID;
        }

        return SingularityResult{alpha, r_sq, state, valid};
    }

    ElementEvidence MovingWindow::buildEvidence(
        const SamplePoint&     point,
        ElementField           field,
        const SpatialContext&  ctx,
        const SingularityResult& sing) const {

        ElementEvidence ev;
        ev.element = field;
        ev.raw_value = getFieldValue(point, field);
        if (hasData(ev.raw_value)) {
            ev.log_value = Math::logTransform(ev.raw_value, field);
        } else {
            ev.log_value = NO_DATA;
        }

        ev.local_median_log = ctx.local.valid ? ctx.local.median : NO_DATA;
        ev.local_stddev_log = ctx.local.valid ? ctx.local.stddev : NO_DATA;
        ev.n_local = ctx.local.n;
        ev.local_radius_km = ctx.local_radius_used;
        ev.local_z = ctx.local_z;

        ev.regional_median_log = ctx.regional.valid ? ctx.regional.median : NO_DATA;
        ev.regional_stddev_log = ctx.regional.valid ? ctx.regional.stddev : NO_DATA;
        ev.n_regional = ctx.regional.n;
        ev.regional_z = ctx.regional_z;

        ev.anomaly_score = ctx.anomaly_score;
        ev.alpha = sing.alpha;
        ev.singularity_state = sing.state;

        ev.is_anomalous = (hasData(ctx.local_z) && ctx.local_z >= ANOMALY_Z_THRESHOLD);
        ev.is_strongly_anomalous = (hasData(ctx.local_z) && ctx.local_z >= STRONG_ANOMALY_Z_THRESHOLD);

        if (point.sample_type == SampleType::VIRTUAL_GRID) {
            ev.data_quality_note = "VIRTUAL_GRID: Z-score computed from IDW-smoothed value. "
                                   "Anomaly intensity may be underestimated vs real sample points. "
                                   "Confirm against nearest real sample if local_z is borderline.";
        } else {
            ev.data_quality_note = "";
        }

        return ev;
    }
}
