#include "data/CSVLoader.h"
#include "utils/Logger.h"
#include "utils/MathUtils.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace GeoIQ {

    std::vector<std::string> CSVLoader::splitLine(const std::string& line) const {
        std::vector<std::string> tokens;
        std::string current;
        bool in_quotes = false;

        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                in_quotes = !in_quotes;
                current += c; // Keep quotes for cleanToken to strip later
            } else if (c == ',' && !in_quotes) {
                tokens.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        tokens.push_back(current);
        return tokens;
    }

    std::string CSVLoader::trim(const std::string& s) const {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, (last - first + 1));
    }

    std::string CSVLoader::cleanToken(const std::string& token) const {
        std::string s = trim(token);
        // TRAP-08: Strip surrounding double quotes from raw CSV cells
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            s = s.substr(1, s.size() - 2);
        }
        return trim(s);
    }

    bool CSVLoader::isNoDataString(const std::string& s) const {
        std::string clean = s;
        std::transform(clean.begin(), clean.end(), clean.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        return (clean.empty() || clean == "na" || clean == "n/a" || clean == "nan" ||
                clean == "-9999" || clean == "-9999.0" || clean == "nd" ||
                clean == "bdl" || clean == "below detection");
    }

    double CSVLoader::parseDouble(const std::string& s, bool allow_negative) const {
        if (isNoDataString(s)) {
            return NO_DATA;
        }

        try {
            size_t processed = 0;
            double val = std::stod(s, &processed);
            if (processed == 0) {
                return NO_DATA;
            }
            if (!allow_negative && val < 0.0) {
                return NO_DATA; // Clamps negative concentrations to NO_DATA
            }
            return val;
        } catch (const std::exception&) {
            return NO_DATA; // Gracefully handles formatting errors without crashing
        }
    }

    SampleType CSVLoader::parseSampleType(const std::string& s) const {
        std::string clean = s;
        std::transform(clean.begin(), clean.end(), clean.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if (clean == "stream_sediment" || clean == "stream sediment" || clean == "sediment") {
            return SampleType::STREAM_SEDIMENT;
        }
        if (clean == "soil") {
            return SampleType::SOIL;
        }
        if (clean == "borehole" || clean == "bore" || clean == "well") {
            return SampleType::BOREHOLE;
        }
        if (clean == "groundwater" || clean == "ground water" || clean == "water") {
            return SampleType::GROUNDWATER;
        }
        if (clean == "rock" || clean == "outcrop") {
            return SampleType::ROCK;
        }
        return SampleType::UNKNOWN;
    }

    HorizonType CSVLoader::parseHorizonType(const std::string& s) const {
        std::string clean = s;
        std::transform(clean.begin(), clean.end(), clean.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if (clean == "surface" || clean == "0-5" || clean == "0–5") {
            return HorizonType::SURFACE;
        }
        if (clean == "a" || clean == "a-horizon" || clean == "ah") {
            return HorizonType::A_HORIZON;
        }
        if (clean == "b" || clean == "b-horizon" || clean == "bh") {
            return HorizonType::B_HORIZON;
        }
        if (clean == "c" || clean == "c-horizon" || clean == "ch") {
            return HorizonType::C_HORIZON;
        }
        if (clean == "borehole" || clean == "core") {
            return HorizonType::BOREHOLE_SAMPLE;
        }
        return HorizonType::UNKNOWN;
    }

    std::vector<SamplePoint> CSVLoader::load(const std::string& filename) {
        // Reset counters and clear caches
        loaded_count_ = 0;
        skipped_count_ = 0;
        missing_columns_.clear();
        found_columns_.clear();

        std::ifstream file(filename);
        if (!file.is_open()) {
            Logger::getInstance().error("CSVLoader", "Cannot open input CSV file: " + filename);
            throw std::runtime_error("Cannot open input CSV file: " + filename);
        }

        Logger::getInstance().info("CSVLoader", "Ingesting file: " + filename);

        // Process Header Row
        std::string header_line;
        if (!std::getline(file, header_line)) {
            Logger::getInstance().error("CSVLoader", "Input CSV file is empty: " + filename);
            throw std::runtime_error("Input CSV file is empty: " + filename);
        }

        std::vector<std::string> raw_headers = splitLine(header_line);
        std::vector<std::string> headers;
        for (const auto& h : raw_headers) {
            std::string cleaned = cleanToken(h);
            found_columns_.push_back(cleaned);
            std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
            headers.push_back(cleaned);
        }

        // Header mapping helper Lambda
        auto find_header = [&](const std::vector<std::string>& candidates) -> int {
            for (const auto& cand : candidates) {
                for (size_t i = 0; i < headers.size(); ++i) {
                    if (headers[i] == cand) {
                        return static_cast<int>(i);
                    }
                }
            }
            return -1;
        };

        // Find primary index targets
        int idx_sample_id = find_header({"sample_id", "id", "sampleid", "sample_no", "sample_number"});
        int idx_lat       = find_header({"lat", "latitude", "y"});
        int idx_lon       = find_header({"lon", "longitude", "long", "x"});

        if (idx_sample_id == -1 || idx_lat == -1 || idx_lon == -1) {
            Logger::getInstance().error("CSVLoader", "Mandatory columns 'sample_id', 'lat', or 'lon' are missing.");
            throw std::runtime_error("Required mandatory columns missing in CSV: " + filename);
        }

        // Find optional columns
        int idx_elev    = find_header({"elevation_m", "elevation", "elev", "alt", "altitude_m"});
        int idx_depth   = find_header({"depth_m", "depth", "well_depth", "sample_depth"});
        int idx_source  = find_header({"source", "dataset", "survey"});
        int idx_type    = find_header({"sample_type", "type", "medium"});
        int idx_horizon = find_header({"horizon", "soil_horizon", "layer"});
        int idx_litho   = find_header({"lithology", "rock_type", "geology"});

        // Oxide conversion tracking flags
        bool convert_fe = false;
        bool convert_al = false;

        // Resolve indices for all 49 element fields
        int idx_elements[static_cast<int>(ElementField::COUNT)];
        for (int i = 0; i < static_cast<int>(ElementField::COUNT); ++i) {
            ElementField f = static_cast<ElementField>(i);
            std::vector<std::string> names;

            if (f == ElementField::Au) {
                names = {"au", "gold", "au_ppb"};
            } else if (f == ElementField::Fe) {
                names = {"fe", "fe_pct", "iron", "fe2o3"};
            } else if (f == ElementField::Al) {
                names = {"al", "al_pct", "aluminium", "aluminum", "al2o3"};
            } else if (f == ElementField::SiO2) {
                names = {"sio2", "silica"};
            } else if (f == ElementField::MgO) {
                names = {"mgo"};
            } else if (f == ElementField::K2O) {
                names = {"k2o"};
            } else if (f == ElementField::Na2O) {
                names = {"na2o"};
            } else if (f == ElementField::CaO) {
                names = {"cao"};
            } else if (f == ElementField::P2O5) {
                names = {"p2o5"};
            } else if (f == ElementField::S) {
                names = {"s", "sulfur", "sulphur"};
            } else if (f == ElementField::LOI) {
                names = {"loi", "loss_on_ignition"};
            } else if (f == ElementField::pH) {
                names = {"ph", "soil_ph", "water_ph"};
            } else if (f == ElementField::Eh) {
                names = {"eh", "redox", "orp"};
            } else if (f == ElementField::elevation_m) {
                names = {"elevation_m", "elevation", "elev", "alt", "altitude_m"};
            } else {
                std::string sym = elementFieldToString(f);
                std::transform(sym.begin(), sym.end(), sym.begin(), [](unsigned char c) {
                    return std::tolower(c);
                });
                names = { sym };
            }

            int resolved_idx = find_header(names);
            idx_elements[i] = resolved_idx;

            if (resolved_idx == -1) {
                missing_columns_.push_back(names.front());
                Logger::getInstance().warn("CSVLoader", "Optional element column '" + names.front() + "' not found. Defs: NO_DATA.");
            } else {
                // Check if we need to convert oxides to elements
                if (f == ElementField::Fe && headers[resolved_idx] == "fe2o3") {
                    convert_fe = true;
                    Logger::getInstance().info("CSVLoader", "Detected Fe2O3 column. Fe values will be converted.");
                }
                if (f == ElementField::Al && headers[resolved_idx] == "al2o3") {
                    convert_al = true;
                    Logger::getInstance().info("CSVLoader", "Detected Al2O3 column. Al values will be converted.");
                }
            }
        }

        std::vector<SamplePoint> results;
        std::string line;
        int line_num = 1; // Header is line 1

        // Process Data Rows
        while (std::getline(file, line)) {
            line_num++;
            std::string trimmed = trim(line);
            if (trimmed.empty()) {
                continue; // Skip empty rows silently
            }

            std::vector<std::string> tokens = splitLine(line);
            if (tokens.size() <= static_cast<size_t>(std::max({idx_sample_id, idx_lat, idx_lon}))) {
                Logger::getInstance().warn("CSVLoader", "Skipping line " + std::to_string(line_num) + ": Row has missing coordinate tokens.");
                skipped_count_++;
                continue;
            }

            // Coordinates checking
            double lat = parseDouble(cleanToken(tokens[idx_lat]), true);
            double lon = parseDouble(cleanToken(tokens[idx_lon]), true);

            if (Math::isNoData(lat) || Math::isNoData(lon) || lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
                Logger::getInstance().warn("CSVLoader", "Skipping line " + std::to_string(line_num) + ": Invalid coordinates values (" + std::to_string(lat) + ", " + std::to_string(lon) + ")");
                skipped_count_++;
                continue;
            }

            SamplePoint p;
            p.lat = lat;
            p.lon = lon;
            p.sample_id = cleanToken(tokens[idx_sample_id]);

            // Optional structural metadata
            if (idx_elev != -1 && idx_elev < static_cast<int>(tokens.size())) {
                p.elevation_m = parseDouble(cleanToken(tokens[idx_elev]), true); // elevation can be negative
            }
            if (idx_depth != -1 && idx_depth < static_cast<int>(tokens.size())) {
                p.depth_m = parseDouble(cleanToken(tokens[idx_depth]), false); // depth is positive
            }
            if (idx_source != -1 && idx_source < static_cast<int>(tokens.size())) {
                p.source = cleanToken(tokens[idx_source]);
            }
            if (idx_type != -1 && idx_type < static_cast<int>(tokens.size())) {
                p.sample_type = parseSampleType(cleanToken(tokens[idx_type]));
            }
            if (idx_horizon != -1 && idx_horizon < static_cast<int>(tokens.size())) {
                p.horizon = parseHorizonType(cleanToken(tokens[idx_horizon]));
            }
            if (idx_litho != -1 && idx_litho < static_cast<int>(tokens.size())) {
                p.lithology = cleanToken(tokens[idx_litho]);
            }

            // Parse all 49 element fields
            for (int i = 0; i < static_cast<int>(ElementField::COUNT); ++i) {
                int col_idx = idx_elements[i];
                if (col_idx != -1 && col_idx < static_cast<int>(tokens.size())) {
                    ElementField f = static_cast<ElementField>(i);
                    bool allow_neg = (f == ElementField::elevation_m || f == ElementField::Eh);
                    double val = parseDouble(cleanToken(tokens[col_idx]), allow_neg);
                    setFieldValue(p, f, val);
                }
            }

            // Apply Oxide-to-Element conversions
            if (convert_fe && hasData(p.Fe)) {
                p.Fe *= 0.6994; // Fe2O3 % to Fe %
            }
            if (convert_al && hasData(p.Al)) {
                p.Al *= 0.5293; // Al2O3 % to Al %
            }

            results.push_back(p);
            loaded_count_++;
        }

        Logger::getInstance().info("CSVLoader", "Load completed. Ingested: " + std::to_string(loaded_count_) + " rows. Skipped: " + std::to_string(skipped_count_) + " rows.");
        return results;
    }
}
