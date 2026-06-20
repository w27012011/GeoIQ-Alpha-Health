#ifndef GEOIQ_GEOJSONWRITER_H
#define GEOIQ_GEOJSONWRITER_H

#include <string>
#include <vector>
#include "utils/Structs.h"

namespace GeoIQ {

    class GeoJSONWriter {
    public:
        // Writes prediction records to a file in GeoJSON format.
        // Generates one Feature per (PredictionRecord x Target) result.
        bool write(const std::string& filepath, const std::vector<PredictionRecord>& records) const;

    private:
        std::string escapeString(const std::string& str) const;
        std::string serializeRecord(const PredictionRecord& rec, const RuleResult& res) const;
    };
}

#endif // GEOIQ_GEOJSONWRITER_H
