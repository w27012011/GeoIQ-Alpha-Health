#ifndef GEOIQ_MOVINGWINDOW_H
#define GEOIQ_MOVINGWINDOW_H

#include <vector>
#include <array>
#include <string>
#include "data/SamplePoint.h"
#include "data/Dataset.h"
#include "utils/Structs.h"

namespace GeoIQ {

    class MovingWindow {
    public:
        // Configuration
        double local_radius_km    = DEFAULT_LOCAL_RADIUS_KM;
        double regional_radius_km = DEFAULT_REGIONAL_RADIUS_KM;

        // Singularity radii in km — the 5 scales (Cheng 2007 recommends multiple scales)
        std::vector<double> singularity_radii = {2.0, 5.0, 10.0, 25.0, 50.0};

        struct Result {
            SpatialContext    context;
            SingularityResult singularity;
            ElementEvidence   evidence;
        };

        Result compute(
            const SamplePoint& point,
            ElementField       field,
            const Dataset&     dataset) const;

        // Convenience: compute for ALL 49 element fields at once.
        std::array<Result, static_cast<size_t>(ElementField::COUNT)> computeAll(
            const SamplePoint&              point,
            const Dataset&                  dataset) const;

    private:
        SpatialContext    computeContext(
            const SamplePoint&                  point,
            ElementField                        field,
            const std::vector<const SamplePoint*>& local_nbrs,
            const std::vector<const SamplePoint*>& regional_nbrs,
            double local_radius_used,
            double regional_radius_used) const;

        SingularityResult computeSingularity(
            const SamplePoint& point,
            ElementField       field,
            const Dataset&     dataset) const;

        ElementEvidence buildEvidence(
            const SamplePoint&     point,
            ElementField           field,
            const SpatialContext&  ctx,
            const SingularityResult& sing) const;
    };
}

#endif // GEOIQ_MOVINGWINDOW_H
