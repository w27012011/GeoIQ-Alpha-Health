#ifndef GEOIQ_SCOREENGINE_H
#define GEOIQ_SCOREENGINE_H

#include <vector>
#include "data/SamplePoint.h"
#include "utils/Structs.h"
#include "spatial/SpatialAnalysis.h"

namespace GeoIQ {

    class ScoreEngine {
    public:
        // Process rule results and dispersion diagnostics for a single point.
        // Compiles them into a finalized PredictionRecord.
        PredictionRecord compile(
            const SamplePoint&               point,
            const std::vector<RuleResult>&   results,
            const SpatialAnalysis::DispersionResult& dispersion) const;

        // Process a collection of points in bulk.
        std::vector<PredictionRecord> compileBatch(
            const std::vector<SamplePoint>&  points,
            const std::vector<std::vector<RuleResult>>& all_results,
            const std::vector<SpatialAnalysis::DispersionResult>& all_dispersions) const;
    };
}

#endif // GEOIQ_SCOREENGINE_H
