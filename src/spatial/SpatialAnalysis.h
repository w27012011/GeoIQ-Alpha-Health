#ifndef GEOIQ_SPATIALANALYSIS_H
#define GEOIQ_SPATIALANALYSIS_H

#include <vector>
#include <string>
#include "data/SamplePoint.h"
#include "data/Dataset.h"
#include "utils/Structs.h"

namespace GeoIQ {

    class SpatialAnalysis {
    public:
        struct ConsistencyResult {
            double consistency      = 0.0;  // 0.0–1.0
            int    n_agreeing       = 0;
            int    n_total          = 0;
            std::string annotation  = "";
        };

        ConsistencyResult spatialConsistency(
            const SamplePoint&                     point,
            ElementField                           field,
            const std::vector<const SamplePoint*>& neighbours,
            double                                 z_threshold = ANOMALY_Z_THRESHOLD) const;

        struct DispersionResult {
            bool   detected             = false;
            double source_bearing_deg   = NO_DATA; // 0–360 degrees from North
            double source_distance_km   = NO_DATA; // estimated km to source
            double gradient_strength    = 0.0;     // how clear the gradient is (0–1)
            std::string annotation      = "";
        };

        DispersionResult detectDispersionTrain(
            const SamplePoint& point,
            ElementField       field,
            const Dataset&     dataset,
            double             search_radius_km = 30.0) const;

    private:
        double sectorMean(
            const SamplePoint&                      query,
            ElementField                            field,
            const std::vector<const SamplePoint*>&  neighbours,
            double bearing_centre_deg,
            double sector_width_deg = 45.0) const;

        bool inSector(
            const SamplePoint& query,
            const SamplePoint& neighbour,
            double bearing_centre_deg,
            double sector_width_deg) const;

        static double getDecayLambda(ElementField field);
        static std::string bearingName(double degrees);
    };
}

#endif // GEOIQ_SPATIALANALYSIS_H
