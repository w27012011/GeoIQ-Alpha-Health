#ifndef GEOIQ_GRIDGENERATOR_H
#define GEOIQ_GRIDGENERATOR_H

#include <vector>
#include "data/SamplePoint.h"
#include "data/Dataset.h"
#include "utils/Constants.h"

namespace GeoIQ {

    class GridGenerator {
    public:
        double cell_size_km          = DEFAULT_GRID_CELL_SIZE_KM;
        double boundary_padding_km   = 5.0;  // padded boundary buffer
        double idw_power             = DEFAULT_IDW_POWER;
        double idw_search_radius_km  = DEFAULT_REGIONAL_RADIUS_KM; // limit IDW neighbors

        // Generates a grid of virtual points based on the bounding box of the dataset.
        // Interpolates values using IDW from dataset.
        std::vector<SamplePoint> generate(const Dataset& dataset) const;
    };
}

#endif // GEOIQ_GRIDGENERATOR_H
