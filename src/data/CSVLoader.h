#ifndef GEOIQ_CSVLOADER_H
#define GEOIQ_CSVLOADER_H

#include <vector>
#include <string>
#include "data/SamplePoint.h"
#include "utils/Enums.h"

namespace GeoIQ {

    class CSVLoader {
    public:
        CSVLoader() = default;

        // Core dataset ingestion method. Returns vector of SamplePoints.
        // Throws std::runtime_error if required columns missing or file inaccessible.
        std::vector<SamplePoint> load(const std::string& filename);

        // Accessors for logging and report summary metrics
        int getLoadedCount() const { return loaded_count_; }
        int getSkippedCount() const { return skipped_count_; }
        const std::vector<std::string>& getMissingColumns() const { return missing_columns_; }
        const std::vector<std::string>& getFoundColumns()    const { return found_columns_; }

    private:
        int loaded_count_  = 0;
        int skipped_count_ = 0;
        std::vector<std::string> missing_columns_;
        std::vector<std::string> found_columns_;

        // Parsing helpers
        std::vector<std::string> splitLine(const std::string& line) const;
        std::string              trim(const std::string& s) const;
        std::string              cleanToken(const std::string& token) const;
        double                   parseDouble(const std::string& s, bool allow_negative = false) const;
        SampleType               parseSampleType(const std::string& s) const;
        HorizonType              parseHorizonType(const std::string& s) const;
        bool                     isNoDataString(const std::string& s) const;
    };
}

#endif // GEOIQ_CSVLOADER_H
