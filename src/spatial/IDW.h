#ifndef GEOIQ_IDW_H
#define GEOIQ_IDW_H

#include <vector>
#include "data/SamplePoint.h"
#include "utils/Constants.h"

namespace GeoIQ {
    namespace IDW {

        // Estimates the value of the specified ElementField at (query_lat, query_lon) using the IDW formula
        double interpolate(
            double query_lat, double query_lon,
            const std::vector<const SamplePoint*>& neighbours,
            ElementField field,
            double power = DEFAULT_IDW_POWER);

        // Interpolates ALL 49 element/parameter fields at once for a query point
        SamplePoint interpolateAllElements(
            double query_lat, double query_lon,
            const std::vector<const SamplePoint*>& neighbours,
            double power = DEFAULT_IDW_POWER);
    }
}

#endif // GEOIQ_IDW_H
