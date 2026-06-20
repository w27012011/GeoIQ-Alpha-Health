#ifndef GEOIQ_HTMLWRITER_H
#define GEOIQ_HTMLWRITER_H

#include <string>
#include <vector>
#include "utils/Structs.h"

namespace GeoIQ {

    class HTMLWriter {
    public:
        // Writes the self-contained map.html containing Leaflet.js code and inline GeoJSON.
        bool write(const std::string& filepath, const std::vector<PredictionRecord>& records) const;
    };
}

#endif // GEOIQ_HTMLWRITER_H
