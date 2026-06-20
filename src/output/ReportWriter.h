#ifndef GEOIQ_REPORTWRITER_H
#define GEOIQ_REPORTWRITER_H

#include <string>
#include <vector>
#include "utils/Structs.h"

namespace GeoIQ {

    class ReportWriter {
    public:
        // Parameters to customize report headers
        std::string input_filename = "unknown.csv";

        // Generates the comprehensive report file from prediction records.
        bool write(
            const std::string& filepath,
            const std::vector<PredictionRecord>& records,
            const std::vector<std::string>& missing_columns,
            int expanded_radius_count,
            int skipped_row_count) const;
    };
}

#endif // GEOIQ_REPORTWRITER_H
