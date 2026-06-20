#define _CRT_SECURE_NO_WARNINGS
#include "output/ReportWriter.h"
#include "utils/MathUtils.h"
#include "utils/Statistics.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <ctime>

namespace GeoIQ {

    namespace {
        struct Finding {
            const PredictionRecord* rec;
            const RuleResult*       res;
        };

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

        std::string getFiredRulesString(const RuleResult& res) {
            std::string rule_ids = "";
            for (size_t i = 0; i < res.triggered_rules.size(); ++i) {
                if (i > 0) rule_ids += ", ";
                rule_ids += res.triggered_rules[i];
            }
            if (rule_ids.empty()) rule_ids = "None";
            return rule_ids;
        }

        std::string formatDouble(double v) {
            if (Math::isNoData(v) || !std::isfinite(v)) return "NO_DATA";
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(4) << v;
            std::string s = ss.str();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') {
                s.pop_back();
            }
            return s;
        }
    }

    bool ReportWriter::write(
        const std::string& filepath,
        const std::vector<PredictionRecord>& records,
        const std::vector<std::string>& missing_columns,
        int expanded_radius_count,
        int skipped_row_count) const {

        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ts_ss;
        // localtime safety guard via _CRT_SECURE_NO_WARNINGS
        ts_ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        std::string timestamp = ts_ss.str();

        // 1. Report Header
        file << "================================================================================\n"
             << "                           GeoIQ ANALYSIS REPORT                                \n"
             << "================================================================================\n"
             << "Generated:     " << timestamp << "\n"
             << "Input file:    " << input_filename << "\n"
             << "Total samples: " << records.size() << "\n"
             << "--------------------------------------------------------------------------------\n\n";

        // Collect critical findings (CRITICAL or HIGH)
        std::vector<Finding> critical_findings;
        for (const auto& rec : records) {
            for (const auto& res : rec.results) {
                if (res.sufficient_data && (res.risk_level == RiskLevel::CRITICAL || res.risk_level == RiskLevel::HIGH)) {
                    critical_findings.push_back({&rec, &res});
                }
            }
        }
        std::sort(critical_findings.begin(), critical_findings.end(), [](const Finding& a, const Finding& b) {
            return a.res->score > b.res->score;
        });

        // 2. CRITICAL FINDINGS
        file << "=== CRITICAL FINDINGS ===\n";
        if (critical_findings.empty()) {
            file << "No critical environmental hazards or high-potential resource targets detected.\n";
        } else {
            for (const auto& f : critical_findings) {
                file << "- " << Stats::targetToString(f.res->target)
                     << " at (" << std::fixed << std::setprecision(6) << f.rec->lat << ", " << f.rec->lon << ") | Sample ID: " << f.rec->sample_id << "\n"
                     << "  Risk Level:      " << Stats::riskLevelToString(f.res->risk_level)
                     << " (Score: " << std::fixed << std::setprecision(3) << f.res->score << ")\n"
                     << "  Annotation:      " << f.res->annotation << "\n"
                     << "  Triggered Rules: " << getFiredRulesString(*f.res) << "\n"
                     << "  Formula:         " << f.res->final_score_formula << "\n"
                     << "  Derivation:      " << f.res->risk_level_derivation << "\n";

                if ((f.rec->sample_type == SampleType::BOREHOLE || f.rec->sample_type == SampleType::GROUNDWATER) &&
                     Math::hasData(f.rec->depth_m)) {
                    file << "  Depth:           " << std::fixed << std::setprecision(1) << f.rec->depth_m << "m (" << f.res->depth_info.zone_name << ")\n";
                }
                if (!f.res->data_quality_note.empty()) {
                    file << "  Data Quality:    " << f.res->data_quality_note << "\n";
                }

                // Evidence
                file << "  Evidence:\n";
                for (const auto& ev : f.res->evidence) {
                    std::string el_name = elementFieldToString(ev.element);
                    std::string formula = Math::formulaString(el_name, ev.raw_value, ev.log_value,
                                                              ev.local_median_log, ev.local_stddev_log, ev.local_z);
                    file << "    * " << el_name << " (" << getUnitString(ev.element) << "): "
                         << "Raw=" << formatDouble(ev.raw_value)
                         << ", Z=" << formatDouble(ev.local_z)
                         << ", Alpha=" << formatDouble(ev.alpha) << "\n"
                         << "      Formula: " << formula << "\n"
                         << "      State:   " << Stats::singularityStateToString(ev.singularity_state) << "\n";
                    if (!ev.data_quality_note.empty()) {
                        file << "      Note:    " << ev.data_quality_note << "\n";
                    }
                }
                file << "\n";
            }
        }
        file << "\n";

        // 3. ENVIRONMENTAL HAZARDS (Arsenic, sorted by score desc)
        std::vector<Finding> arsenic_hazards;
        for (const auto& rec : records) {
            for (const auto& res : rec.results) {
                if (res.sufficient_data && res.target == Target::ARSENIC_HAZARD) {
                    arsenic_hazards.push_back({&rec, &res});
                }
            }
        }
        std::sort(arsenic_hazards.begin(), arsenic_hazards.end(), [](const Finding& a, const Finding& b) {
            return a.res->score > b.res->score;
        });

        file << "=== ENVIRONMENTAL HAZARDS ===\n";
        if (arsenic_hazards.empty()) {
            file << "No arsenic anomalies detected.\n";
        } else {
            for (const auto& f : arsenic_hazards) {
                file << "- ARSENIC_HAZARD at (" << std::fixed << std::setprecision(6) << f.rec->lat << ", " << f.rec->lon << ") | Sample ID: " << f.rec->sample_id << "\n"
                     << "  Risk Level:      " << Stats::riskLevelToString(f.res->risk_level)
                     << " (Score: " << std::fixed << std::setprecision(3) << f.res->score << ")\n"
                     << "  Annotation:      " << f.res->annotation << "\n";
                if (!f.res->data_quality_note.empty()) {
                    file << "  Data Quality:    " << f.res->data_quality_note << "\n";
                }
                file << "\n";
            }
        }
        file << "\n";

        // 4. RESOURCE POTENTIAL
        file << "=== RESOURCE POTENTIAL ===\n";
        std::vector<Target> resource_targets = {
            Target::GOLD, Target::COPPER_PORPHYRY, Target::COPPER_VMS,
            Target::DIAMOND_KIMBERLITE, Target::LITHIUM_PEGMATITE,
            Target::LITHIUM_BRINE, Target::REE_CARBONATITE, Target::GOSSAN,
            Target::ZINC_LEAD
        };

        for (auto target : resource_targets) {
            std::vector<Finding> group;
            for (const auto& rec : records) {
                for (const auto& res : rec.results) {
                    if (res.sufficient_data && res.target == target && res.risk_level >= RiskLevel::MEDIUM) {
                        group.push_back({&rec, &res});
                    }
                }
            }
            std::sort(group.begin(), group.end(), [](const Finding& a, const Finding& b) {
                return a.res->score > b.res->score;
            });

            file << "Target: " << Stats::targetToString(target) << "\n";
            if (group.empty()) {
                file << "  No significant potential detected.\n\n";
            } else {
                for (const auto& f : group) {
                    file << "  - (" << std::fixed << std::setprecision(6) << f.rec->lat << ", " << f.rec->lon << ") | Sample ID: " << f.rec->sample_id << "\n"
                         << "    Potential Level: " << Stats::riskLevelToString(f.res->risk_level)
                         << " (Score: " << std::fixed << std::setprecision(3) << f.res->score << ")\n"
                         << "    Annotation:      " << f.res->annotation << "\n";
                    if (!f.res->data_quality_note.empty()) {
                        file << "    Data Quality:    " << f.res->data_quality_note << "\n";
                    }
                }
                file << "\n";
            }
        }

        // 5. DATA QUALITY NOTES
        int insufficient_count = 0;
        for (const auto& rec : records) {
            for (const auto& res : rec.results) {
                if (!res.sufficient_data) {
                    insufficient_count++;
                }
            }
        }

        file << "=== DATA QUALITY NOTES ===\n";
        file << "Missing Element Columns: ";
        if (missing_columns.empty()) {
            file << "None\n";
        } else {
            for (size_t i = 0; i < missing_columns.size(); ++i) {
                file << missing_columns[i];
                if (i + 1 < missing_columns.size()) file << ", ";
            }
            file << "\n";
        }
        file << "Radius Expansion Events:           " << expanded_radius_count << "\n";
        file << "Skipped Row Count (Bad Coords):    " << skipped_row_count << "\n";
        file << "Insufficient Data Target Evals:    " << insufficient_count << "\n";

        bool missing_critical = false;
        std::string crit_list = "";
        for (const auto& col : missing_columns) {
            if (col == "As" || col == "Fe" || col == "Au" || col == "Sb" || col == "Cr" || col == "Ni" || col == "Li") {
                missing_critical = true;
                if (!crit_list.empty()) crit_list += ", ";
                crit_list += col;
            }
        }
        if (missing_critical) {
            file << "WARNING: Critical pathfinder elements (" << crit_list << ") are missing from input dataset. Rule execution may be severely limited.\n";
        }
        file << "\n";

        // 6. SCIENTIFIC NOTES & DISCLAIMERS
        file << "=== SCIENTIFIC NOTES & DISCLAIMERS ===\n"
             << "Disclaimer: GeoIQ is an algorithmic decision support system. Ground truth verification\n"
             << "(drilling, trenching, lab assays) is mandatory before planning investments.\n\n"
             << "Literature References:\n"
             << "1. Smedley, P.L. & Kinniburgh, D.G. (2002), Applied Geochemistry 17:517-568.\n"
             << "   A comprehensive review of controls on arsenic in groundwater.\n"
             << "2. Cheng, Q. (2007), Ore Geology Reviews 32:314-324.\n"
             << "   Geochemical singularity analysis and power-law local enrichment models.\n"
             << "3. Economic Geology Pathfinder Guidelines. Society of Economic Geologists (SEG).\n"
             << "   Mineral systems exploration geochemical vectoring models.\n"
             << "================================================================================\n";

        file.close();
        return true;
    }
}
