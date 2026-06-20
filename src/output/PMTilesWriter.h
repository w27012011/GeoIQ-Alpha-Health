#ifndef GEOIQ_PMTILESWRITER_H
#define GEOIQ_PMTILESWRITER_H

#include <string>
#include <vector>
#include "utils/Structs.h"

namespace GeoIQ {

    class PMTilesWriter {
    public:
        // Encodes prediction records as gzipped Mapbox Vector Tiles (MVT) at zoom levels 0-14,
        // and packages them into a cloud-optimized PMTiles v3 archive.
        bool write(const std::string& output_path, const std::vector<PredictionRecord>& records, bool is_geology);
    };

}

#endif // GEOIQ_PMTILESWRITER_H
