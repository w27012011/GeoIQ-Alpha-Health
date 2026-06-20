#pragma once
// =============================================================================
// HealthReader.h  —  GeoIQ HTML Frontend Generator
// Reads audit_health.db (SQLite) via embedded sqlite3.
// Only loads predictions where risk_level != 'LOW' to keep HTML file sizes
// manageable. GeoIQ.exe is NOT modified.
//
// SQL executed:
//   SELECT s.sample_id, s.lat, s.lon,
//          p.prediction_id, p.target, p.risk_level, p.score,
//          p.annotation, p.triggered_rules, p.data_quality_note
//   FROM   predictions p
//   JOIN   samples     s ON p.sample_id = s.sample_id
//   WHERE  p.risk_level != 'LOW'
//   ORDER  BY p.risk_level DESC, p.score DESC
//
//   Then for each prediction_id, evidence is loaded from the evidence table.
// =============================================================================
#include "DataTypes.h"
#include "../vendor/sqlite3.h"
#include <iostream>
#include <sstream>
#include <cstring>

// ─── Split comma-separated string of rule names ───────────────────────────────
inline std::vector<std::string> splitRules(const char* s) {
    std::vector<std::string> out;
    if (!s || *s == '\0') return out;
    std::string token;
    for (; *s; ++s) {
        if (*s == ',') {
            std::string t = token;
            // trim
            auto b = t.find_first_not_of(" []\"");
            auto e = t.find_last_not_of(" []\"");
            if (b != std::string::npos) out.push_back(t.substr(b, e-b+1));
            token.clear();
        } else {
            token += *s;
        }
    }
    if (!token.empty()) {
        auto b = token.find_first_not_of(" []\"");
        auto e = token.find_last_not_of(" []\"");
        if (b != std::string::npos) out.push_back(token.substr(b, e-b+1));
    }
    return out;
}

// ─── Extract rules from annotation string ─────────────────────────────────────
inline std::vector<std::string> extractRulesFromAnnotation(const std::string& ann) {
    std::vector<std::string> rules;
    size_t pos = ann.find("rules: [");
    if (pos != std::string::npos) {
        size_t start = pos + 8;
        size_t end = ann.find("]", start);
        if (end != std::string::npos) {
            std::string rulesStr = ann.substr(start, end - start);
            std::stringstream ss(rulesStr);
            std::string rule;
            while (std::getline(ss, rule, ',')) {
                size_t first = rule.find_first_not_of(" \t\r\n'\"");
                size_t last = rule.find_last_not_of(" \t\r\n'\"");
                if (first != std::string::npos && last != std::string::npos) {
                    rules.push_back(rule.substr(first, last - first + 1));
                }
            }
        }
    }
    return rules;
}

// ─── Load health records from audit_health.db ────────────────────────────────
inline std::vector<HealthRecord> loadHealthRecords(const std::string& dbPath) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[HealthReader] Cannot open " << dbPath
                  << ": " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return {};
    }

    std::unordered_map<std::string, HealthRecord> recordMap;

    // ── Step 1: Load all non-LOW predictions ─────────────────────────────────
    const char* sql = R"(
        SELECT s.sample_id, s.lat, s.lon,
               p.prediction_id, p.target, p.risk_level, p.score,
               p.annotation, s.data_quality_note
        FROM   predictions p
        JOIN   samples     s ON p.sample_id = s.sample_id
        WHERE  p.risk_level != 'LOW'
        ORDER  BY p.score DESC
        LIMIT  2000
    )";

    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[HealthReader] Prepare failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return {};
    }

    // Map: prediction_id → (sample_id, target_index)
    std::unordered_map<int, std::pair<std::string, int>> predIdMap;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* sid  = (const char*)sqlite3_column_text(stmt, 0);
        double lat        = sqlite3_column_double(stmt, 1);
        double lon        = sqlite3_column_double(stmt, 2);
        int    pred_id    = sqlite3_column_int(stmt, 3);
        const char* tgt  = (const char*)sqlite3_column_text(stmt, 4);
        const char* rl   = (const char*)sqlite3_column_text(stmt, 5);
        double score      = sqlite3_column_double(stmt, 6);
        const char* ann  = (const char*)sqlite3_column_text(stmt, 7);
        const char* dqn  = (const char*)sqlite3_column_text(stmt, 8);

        if (!sid) continue;
        std::string sampleId(sid);
        std::string annotationStr = ann ? ann : "";

        HealthRecord& hr = recordMap[sampleId];
        if (hr.sample_id.empty()) {
            hr.sample_id = sampleId;
            hr.lat = lat;
            hr.lon = lon;
        }

        HealthTargetResult tr;
        tr.target           = tgt  ? tgt  : "";
        tr.risk_level       = rl   ? rl   : "LOW";
        tr.score            = score;
        tr.annotation       = annotationStr;
        tr.data_quality_note= dqn  ? dqn  : "";
        tr.triggered_rules  = extractRulesFromAnnotation(annotationStr);

        // Track worst risk
        if (riskOrder(tr.risk_level) > riskOrder(hr.worst_risk_level)) {
            hr.worst_risk_level = tr.risk_level;
        }

        int tIdx = (int)hr.targets.size();
        hr.targets.push_back(std::move(tr));
        predIdMap[pred_id] = {sampleId, tIdx};
    }
    sqlite3_finalize(stmt);

    // ── Step 2: Load evidence for those prediction IDs ────────────────────────
    if (!predIdMap.empty()) {
        const char* evSql = R"(
            SELECT prediction_id, element, raw_value, local_z, regional_z,
                   alpha, singularity_state, formula
            FROM   evidence
            WHERE  prediction_id IN (SELECT prediction_id FROM predictions WHERE risk_level != 'LOW')
            LIMIT  50000
        )";

        sqlite3_stmt* evStmt = nullptr;
        rc = sqlite3_prepare_v2(db, evSql, -1, &evStmt, nullptr);
        if (rc == SQLITE_OK) {
            while (sqlite3_step(evStmt) == SQLITE_ROW) {
                int pid = sqlite3_column_int(evStmt, 0);
                auto it = predIdMap.find(pid);
                if (it == predIdMap.end()) continue;

                const std::string& sid = it->second.first;
                int tIdx = it->second.second;

                auto rIt = recordMap.find(sid);
                if (rIt == recordMap.end()) continue;
                if (tIdx >= (int)rIt->second.targets.size()) continue;

                EvidenceItem ev;
                const char* el = (const char*)sqlite3_column_text(evStmt, 1);
                ev.element = el ? el : "";
                ev.raw_value  = sqlite3_column_type(evStmt,2)==SQLITE_NULL ? NO_DATA_VAL : sqlite3_column_double(evStmt,2);
                ev.local_z    = sqlite3_column_type(evStmt,3)==SQLITE_NULL ? NO_DATA_VAL : sqlite3_column_double(evStmt,3);
                ev.regional_z = sqlite3_column_type(evStmt,4)==SQLITE_NULL ? NO_DATA_VAL : sqlite3_column_double(evStmt,4);
                ev.alpha      = sqlite3_column_type(evStmt,5)==SQLITE_NULL ? NO_DATA_VAL : sqlite3_column_double(evStmt,5);
                const char* ss = (const char*)sqlite3_column_text(evStmt,6);
                ev.singularity_state = ss ? ss : "INVALID";
                const char* fm = (const char*)sqlite3_column_text(evStmt,7);
                ev.formula = fm ? fm : "";

                // Infer unit from element
                if (ev.element == "Au") ev.unit = "ppb";
                else if (ev.element == "Fe" || ev.element == "Al" ||
                         ev.element == "SiO2" || ev.element == "MgO" ||
                         ev.element == "K2O"  || ev.element == "Na2O" ||
                         ev.element == "CaO"  || ev.element == "P2O5" ||
                         ev.element == "S"    || ev.element == "LOI")   ev.unit = "%";
                else if (ev.element == "pH") ev.unit = "pH";
                else if (ev.element == "Eh") ev.unit = "mV";
                else ev.unit = "ppm";

                rIt->second.targets[tIdx].evidence.push_back(std::move(ev));
            }
            sqlite3_finalize(evStmt);
        }
    }

    sqlite3_close(db);

    // Flatten to vector
    std::vector<HealthRecord> result;
    result.reserve(recordMap.size());
    for (auto& kv : recordMap) result.push_back(std::move(kv.second));

    std::cout << "[HealthReader] Loaded " << result.size()
              << " elevated-risk health locations from " << dbPath << "\n";
    return result;
}
