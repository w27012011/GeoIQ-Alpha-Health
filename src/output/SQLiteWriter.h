#ifndef GEOIQ_SQLITEWRITER_H
#define GEOIQ_SQLITEWRITER_H

#include <string>
#include <vector>
#include "utils/Structs.h"

namespace GeoIQ {

    class SQLiteWriter {
    public:
        // Main entry point. Opens db_path, runs DDL, seeds known mines, and inserts records.
        bool write(const std::string& db_path, const std::vector<PredictionRecord>& records, bool is_geology);
    };

}

#endif // GEOIQ_SQLITEWRITER_H
