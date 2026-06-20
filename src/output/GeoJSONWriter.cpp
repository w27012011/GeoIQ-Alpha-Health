#include "output/GeoJSONWriter.h"
#include "utils/MathUtils.h"
#include "utils/Statistics.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace GeoIQ {

    namespace {
        std::string formatDouble(double v) {
            if (Math::isNoData(v) || !std::isfinite(v)) return "null";
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(6) << v;
            std::string s = ss.str();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') {
                s.pop_back();
            }
            return s;
        }

        std::string getUnitString(ElementField field) {
            if (field == ElementField::Au) return "ppb";
            int idx = static_cast<int>(field);
            if (idx >= static_cast<int>(ElementField::Fe) && idx <= static_cast<int>(ElementField::LOI)) {
                return "%";
            }
            if (field == ElementField::pH) return "pH";
            if (field == ElementField::Eh) return "mV";
            if (field == ElementField::elevation_m) return "m";
            return "ppm";
        }
    }

    bool GeoJSONWriter::write(const std::string& filepath, const std::vector<PredictionRecord>& records) const {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        file << "{\n"
             << "  \"type\": \"FeatureCollection\",\n"
             << "  \"features\": [\n";

        bool first_feature = true;
        for (const auto& rec : records) {
            for (const auto& res : rec.results) {
                if (!first_feature) {
                    file << ",\n";
                }
                first_feature = false;
                file << serializeRecord(rec, res);
            }
        }

        file << "\n  ]\n"
             << "}\n";

        file.close();
        return true;
    }

    std::string GeoJSONWriter::escapeString(const std::string& str) const {
        std::string s = "";
        s.reserve(str.size());
        for (char c : str) {
            if (c == '"') {
                s += "\\\"";
            } else if (c == '\\') {
                s += "\\\\";
            } else if (c == '\n') {
                s += "\\n";
            } else if (c == '\r') {
                s += "\\r";
            } else if (c == '\t') {
                s += "\\t";
            } else {
                s += c;
            }
        }
        return s;
    }

    std::string GeoJSONWriter::serializeRecord(const PredictionRecord& rec, const RuleResult& res) const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(6);

        ss << "    {\n"
           << "      \"type\": \"Feature\",\n"
           << "      \"geometry\": {\n"
           << "        \"type\": \"Point\",\n"
           << "        \"coordinates\": [" << rec.lon << ", " << rec.lat << "]\n"
           << "      },\n"
           << "      \"properties\": {\n"
           << "        \"sample_id\": \"" << escapeString(rec.sample_id) << "\",\n"
           << "        \"depth_m\": " << formatDouble(rec.depth_m) << ",\n"
           << "        \"elevation_m\": " << formatDouble(rec.elevation_m) << ",\n"
           << "        \"target\": \"" << Stats::targetToString(res.target) << "\",\n"
           << "        \"risk_level\": \"" << Stats::riskLevelToString(res.risk_level) << "\",\n"
           << "        \"score\": " << formatDouble(res.score) << ",\n"
           << "        \"confidence\": " << formatDouble(res.confidence) << ",\n"
           << "        \"annotation\": \"" << escapeString(res.annotation) << "\",\n";

        // triggered_rules
        ss << "        \"triggered_rules\": [";
        for (size_t i = 0; i < res.triggered_rules.size(); ++i) {
            ss << "\"" << escapeString(res.triggered_rules[i]) << "\"";
            if (i + 1 < res.triggered_rules.size()) ss << ", ";
        }
        ss << "],\n";

        // depth_zone
        if (res.depth_info.applicable && !res.depth_info.zone_name.empty()) {
            ss << "        \"depth_zone\": \"" << escapeString(res.depth_info.zone_name) << "\",\n";
        } else {
            ss << "        \"depth_zone\": null,\n";
        }

        // spatial consistency, dispersion, quality notes
        ss << "        \"spatial_consistency\": " << formatDouble(res.spatial_consistency) << ",\n"
           << "        \"dispersion_detected\": " << (rec.dispersion_detected ? "true" : "false") << ",\n"
           << "        \"source_bearing_deg\": " << formatDouble(rec.source_bearing_deg) << ",\n"
           << "        \"source_distance_km\": " << formatDouble(rec.source_distance_km) << ",\n"
           << "        \"data_quality_note\": \"" << escapeString(res.data_quality_note) << "\",\n";

        // evidence
        ss << "        \"evidence\": [\n";
        for (size_t i = 0; i < res.evidence.size(); ++i) {
            const auto& ev = res.evidence[i];
            std::string el_name = elementFieldToString(ev.element);
            std::string formula = Math::formulaString(el_name, ev.raw_value, ev.log_value,
                                                      ev.local_median_log, ev.local_stddev_log, ev.local_z);

            ss << "          {\n"
               << "            \"element\": \"" << el_name << "\",\n"
               << "            \"unit\": \"" << getUnitString(ev.element) << "\",\n"
               << "            \"raw_value\": " << formatDouble(ev.raw_value) << ",\n"
               << "            \"local_z\": " << formatDouble(ev.local_z) << ",\n"
               << "            \"regional_z\": " << formatDouble(ev.regional_z) << ",\n"
               << "            \"alpha\": " << formatDouble(ev.alpha) << ",\n"
               << "            \"singularity_state\": \"" << Stats::singularityStateToString(ev.singularity_state) << "\",\n"
               << "            \"formula\": \"" << escapeString(formula) << "\",\n"
               << "            \"data_quality_note\": \"" << escapeString(ev.data_quality_note) << "\"\n"
               << "          }";
            if (i + 1 < res.evidence.size()) ss << ",\n";
            else ss << "\n";
        }
        ss << "        ]\n"
           << "      }\n"
           << "    }";

        return ss.str();
    }
}
