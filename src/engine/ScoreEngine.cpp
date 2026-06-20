#include "engine/ScoreEngine.h"
#include <algorithm>

namespace GeoIQ {

    PredictionRecord ScoreEngine::compile(
        const SamplePoint&               point,
        const std::vector<RuleResult>&   results,
        const SpatialAnalysis::DispersionResult& dispersion) const {

        PredictionRecord rec;
        rec.lat         = point.lat;
        rec.lon         = point.lon;
        rec.elevation_m = point.elevation_m;
        rec.depth_m     = point.depth_m;
        rec.sample_id   = point.sample_id;
        rec.sample_type = point.sample_type;

        std::vector<RuleResult> consolidated_results;
        consolidated_results.reserve(results.size());

        for (auto res : results) {
            if (!res.sufficient_data) {
                continue; // Skip targets with insufficient data
            }

            // Target Consolidation: COPPER_PORPHYRY to COPPER_VMS if CU-3 fired
            if (res.target == Target::COPPER_PORPHYRY) {
                auto it = std::find(res.triggered_rules.begin(), res.triggered_rules.end(), "CU-3");
                if (it != res.triggered_rules.end()) {
                    res.target = Target::COPPER_VMS;
                    res.annotation = "[VMS] copper VMS potential. Fired rule: CU-3 massive sulfide.";
                }
            }

            // Target Consolidation: LITHIUM_PEGMATITE to LITHIUM_BRINE if LI-2 fired
            if (res.target == Target::LITHIUM_PEGMATITE) {
                auto it = std::find(res.triggered_rules.begin(), res.triggered_rules.end(), "LI-2");
                if (it != res.triggered_rules.end()) {
                    res.target = Target::LITHIUM_BRINE;
                }
            }

            consolidated_results.push_back(std::move(res));
        }

        rec.results = std::move(consolidated_results);

        rec.dispersion_detected   = dispersion.detected;
        rec.source_bearing_deg    = dispersion.source_bearing_deg;
        rec.source_distance_km    = dispersion.source_distance_km;
        rec.dispersion_annotation = dispersion.annotation;

        return rec;
    }

    std::vector<PredictionRecord> ScoreEngine::compileBatch(
        const std::vector<SamplePoint>&  points,
        const std::vector<std::vector<RuleResult>>& all_results,
        const std::vector<SpatialAnalysis::DispersionResult>& all_dispersions) const {

        std::vector<PredictionRecord> batch;
        batch.reserve(points.size());

        for (size_t i = 0; i < points.size(); ++i) {
            batch.push_back(compile(points[i], all_results[i], all_dispersions[i]));
        }

        return batch;
    }
}
